#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <event.h>
#include <errno.h>

#include "params.h"
#include "dispatcher.h"
#include "balancer.h"
#include "metrics.h"

struct connection {
    int num;
    enum state state;
    int socket;
    int error;
    int written;
    struct location *location; /* currently fetching from this location */
    char buf[BUFSIZ];
    ssize_t nbytes; /* number of bytes read into buffer */
    ssize_t responselen; /* total bytes read for the response */
    float http_version;
    unsigned int resp_code; /* response code */
    const char *resp_str; /* response string */
    struct metrics metrics; /* metrics for this request */
};

static struct metrics global_metrics;

static int n_dispatched = 0;

static struct timeval tvnow = { 0, 0 };

static struct connection *connections;

static void process_state(struct connection *conn);

void process_error(struct connection *conn)
{
    conn->location->n_errors++;
    if (config_opts.verbose > 0)
        fprintf(stderr, "socket failure %d (%s) connecting to %s\n",
                conn->error, strerror(conn->error),
                conn->location->uri->hostname);
    conn->state = ST_CLEANUP;
    process_state(conn);
}

void process_cleanup(struct connection *conn)
{
    memset(conn, 0, sizeof(*conn)); // clear the memory
    process_state(conn); // process the new idle state
}

void process_calculating(struct connection *conn)
{
    /* add metrics from this run to this location's total */
    metrics_accumulate(&conn->location->total_metrics, &conn->metrics);

    /* add metrics from this run to global total */
    metrics_accumulate(&global_metrics, &conn->metrics);

    conn->state = ST_CLEANUP;
    process_state(conn);
}

void process_closing(struct connection *conn)
{
    int rc = close(conn->socket);
    if (rc < 0) {
        conn->error = errno;
        conn->state = ST_ERROR;
        if (config_opts.verbose > 0)
            fprintf(stderr, "error closing fd %d: %s\n",
                    conn->socket, strerror(conn->error));
    } else {
        conn->state = ST_CLOSED;
    }
    process_state(conn);
}

void process_reading_body(int fd, short event, void *_conn)
{
    struct connection *conn = (struct connection *)_conn;
    ssize_t count;
    int e;
    char buf[BUFSIZ];

retry:
    count = read(fd, buf, sizeof(buf));
    e = errno;
    if (count < 0) {
        if (e == EINTR) {
            if (config_opts.verbose > 3)
                fprintf(stderr, "read(%d) received EINTR, retrying\n", fd);
            goto retry;
        } else if (e == EAGAIN || e == EWOULDBLOCK) {
            if (config_opts.verbose > 3)
                fprintf(stderr, "read(%d) received EAGAIN, sleeping\n", fd);
            goto out;
        }
    } else if (count == 0) { // EOF
        conn->state = ST_READ;
        if (config_opts.verbose > 4)
            fprintf(stderr, "fd %d done reading body, received %ld bytes total\n", fd, conn->responselen);
    } else {
        conn->responselen += count;
        if (config_opts.verbose > 5)
            fprintf(stderr, "fd %d read %ld body bytes...\n", fd, count);
        goto retry;
    }

out:
    process_state(conn);
}

void process_reading_header(int fd, short event, void *_conn)
{
    struct connection *conn = (struct connection *)_conn;
    ssize_t count;
    int e;

retry:
    count = read(fd, conn->buf + conn->nbytes,
                     sizeof(conn->buf) - conn->nbytes - 1);
                     // leave space for the \0
    e = errno;
    if (count < 0) {
        if (e == EINTR) {
            if (config_opts.verbose > 3)
                fprintf(stderr, "read(%d) received EINTR, retrying\n", fd);
            goto retry;
        } else if (e == EAGAIN || e == EWOULDBLOCK) {
            if (config_opts.verbose > 3)
                fprintf(stderr, "read(%d) received EAGAIN, sleeping\n", fd);
            goto out;
        }
    } else if (count == 0) { // EOF
        // error, because we didn't find the \r\n on a previous call
        if (config_opts.verbose > 0)
            fprintf(stderr, "premature EOF on fd %d\n", fd);
        conn->error = EPIPE;
        conn->state = ST_ERROR;
        goto out;
    } else { // successful read, not sure if we have everything yet
        char *end = strstr(conn->buf + conn->nbytes, "\r\n");
        conn->responselen += count;
        conn->nbytes += count;
        if (end == NULL) { // not found
            if (conn->nbytes - 1 == sizeof(conn->buf)) { // overflow
                if (config_opts.verbose > 0)
                    fprintf(stderr, "fd %d header too long\n", fd);
                conn->error = ENOMEM;
                conn->state = ST_ERROR;
                goto out;
            } else { // not done yet, try again
                if (config_opts.verbose > 4)
                    fprintf(stderr, "fd %d received %ld bytes, no header found yet\n", fd, count);
                goto retry;
            }
        } else { // response code header found
            int prefixlen = 0;
            count = sscanf(conn->buf, "HTTP/%3f %u %n",
                           &conn->http_version, &conn->resp_code, &prefixlen);
            if (count != 2) {
                if (config_opts.verbose > 1)
                    fprintf(stderr, "error parsing response header from fd %d\n", fd);
                conn->state = ST_ERROR;
                conn->error = EINVAL;
                goto out;
            }
            *end = '\0'; // set \r\n to NULL
            conn->resp_str = conn->buf + prefixlen;
            if (config_opts.verbose > 5)
                fprintf(stderr, "fd %d returned response code %u and string %s\n", fd, conn->resp_code, conn->resp_str);
            conn->state = ST_READING_BODY;
            goto out;
        }
    }

out:
    process_state(conn);
}

void process_written(struct connection *conn)
{
    if (shutdown(conn->socket, SHUT_WR) < 0) {
        conn->error = errno;
        conn->state = ST_ERROR;
        fprintf(stderr, "error shutting down fd %d for writes: %s\n",
                conn->socket, strerror(conn->error));
    } else {
        conn->state = ST_READING_HEADER;
    }
    process_state(conn);
}

void process_writing(int fd, short event, void *_conn)
{
    struct connection *conn = (struct connection *)_conn;
    ssize_t count;
    int e;

retry_write:
    count = write(fd, conn->location->request + conn->written,
                  conn->location->rlen - conn->written);
    e = errno;
    if (count < 0 && e == EINTR) {
        if (config_opts.verbose > 3)
            fprintf(stderr, "write(%d) received EINTR, retrying\n", fd);
        goto retry_write;
    } else if (count < 0 && (e == EAGAIN || e == EWOULDBLOCK)) {
        conn->state = ST_WRITING; /* come back later */
        if (config_opts.verbose > 3)
            fprintf(stderr, "write(%d) received EAGAIN, sleeping\n", fd);
        goto out;
    } else if (count < 0) {
        /* some error occurred */
        if (config_opts.verbose > 3)
            fprintf(stderr, "write(%d) received error %s, sleeping\n", fd,
                    strerror(e));
        conn->error = e;
        conn->state = ST_ERROR;
        goto out;
    } else if (count == conn->location->rlen - conn->written) {
        /* done writing */
        if (config_opts.verbose > 4)
            fprintf(stderr, "write(%d) wrote full request, len %ld: '%s'\n",
                    fd, count, conn->location->request + conn->written);
        else if (config_opts.verbose > 3)
            fprintf(stderr, "write(%d) wrote full request, len %ld\n", fd,
                    count);
        conn->written = count;
        conn->state = ST_WRITTEN;
        goto out;
    } else {
        /* some or none was written, try again while still writable */
        if (config_opts.verbose > 3)
            fprintf(stderr, "write(%d) wrote %ld bytes, trying again\n", fd,
                    count);
        conn->written += count;
        goto retry_write;
    }

out:
    process_state(conn);
}

void process_connecting(int fd, short event, void *_conn)
{
    struct connection *conn = (struct connection *)_conn;
    int error;
    socklen_t len;

    /* catch timeouts and send them to the timeout state */
    if (event & EV_TIMEOUT) {
        conn->state = ST_TIMEOUT;
        goto out;
    }

    len = sizeof(error);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
        /* solaris only section, or getsockopt() error on BSD */
        conn->error = errno;
        conn->state = ST_ERROR;
    } else if (error) {
        /* normal socket error, BSD-style */
        conn->error = error;
        conn->state = ST_ERROR;
    } else {
        /* no error, ready to read */
        conn->state = ST_CONNECTED;
    }

out:
    process_state(conn);
}

void process_idle(int fd, short event, void *_conn)
    /* input fd is ignored */
{
    struct connection *conn = (struct connection *)_conn;
    int flags, rv;

    if (n_dispatched > config_opts.count) {
        if (config_opts.verbose > 1)
            fprintf(stderr, "Finished dispatching %dth request\n",
                    n_dispatched);
        return; /* done */
    }

    /* create socket */
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        exit(-3);
    }
    if (config_opts.verbose > 3)
        fprintf(stderr, "Created socket %d\n", fd);

    /* set to O_NONBLOCK */
    flags = fcntl(fd, F_GETFL, 0);
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("fcntl O_NONBLOCK");
        exit(-3);
    }
    if (config_opts.verbose > 3)
        fprintf(stderr, "Set socket %d to O_NONBLOCK\n", fd);

    /* save the socket for later */
    conn->socket = fd;

    /* connect to the socket */
    conn->location = get_next_location();
    rv = location_connect(conn->location, fd);
    if (rv < 0) {
        perror("connect_next failed");
        exit(-3);
    } else if (rv == 0) {
        /* we were able to complete the connect immediately, no waiting */
        conn->state = ST_CONNECTED;
    } else {
        /* we weren't able to complete the connect immediately, check later */
        conn->state = ST_CONNECTING;
    }

    n_dispatched++; /* assumes non-reentrancy throughout above function */
    process_state(conn);
}

static void process_state(struct connection *conn)
{
    /* FIXME: change all event_once calls to event_add, for efficiency */
    if (config_opts.verbose > 4) {
        char *state_str;
        switch (conn->state) {
            case ST_IDLE:
                state_str = "ST_IDLE"; break;
            case ST_CONNECTING:
                state_str = "ST_CONNECING"; break;
            case ST_CONNECTED:
                state_str = "ST_CONNECTED"; break;
            case ST_WRITING:
                state_str = "ST_WRITING"; break;
            case ST_WRITTEN:
                state_str = "ST_WRITTEN"; break;
            case ST_READING_HEADER:
                state_str = "ST_READING_HEADER"; break;
            case ST_READING_BODY:
                state_str = "ST_READING_BODY"; break;
            case ST_READ:
                state_str = "ST_READ"; break;
            case ST_CLOSING:
                state_str = "ST_CLOSING"; break;
            case ST_CLOSED:
                state_str = "ST_CLOSED"; break;
            case ST_CALCULATING:
                state_str = "ST_CALCULATING"; break;
            case ST_CLEANUP:
                state_str = "ST_CLEANUP"; break;
            case ST_TIMEOUT:
                state_str = "ST_TIMEOUT"; break;
            case ST_ERROR:
                state_str = "ST_ERROR"; break;
            default:
                state_str = "(UNKNOWN)"; break;
        };
        fprintf(stderr, "Processing fd %d with state %s\n", conn->socket,
                state_str);
    }
    switch (conn->state) {
        case ST_IDLE:
            /* schedule for immediate call */
            // FIXME: maybe call this directly instead of schedule it
            event_once(0, EV_TIMEOUT, process_idle, conn, &tvnow);
            break;
        case ST_CONNECTING:
            /* FIXME: specify a connect timeout, configured globally */
            event_once(conn->socket, EV_READ|EV_WRITE, process_connecting,
                       conn, NULL);
            break;
        case ST_CONNECTED:
            // fall through to the next state
        case ST_WRITING:
            event_once(conn->socket, EV_WRITE, process_writing, conn, NULL);
            break;
        case ST_WRITTEN:
            process_written(conn);
            break;
        case ST_READING_HEADER:
            event_once(conn->socket, EV_READ, process_reading_header, conn, NULL);
            break;
        case ST_READING_BODY:
            event_once(conn->socket, EV_READ, process_reading_body, conn, NULL);
            break;
        case ST_READ:
            // fall through to the next state
        case ST_CLOSING:
            process_closing(conn);
            break;
        case ST_CLOSED:
            // fall through to the next state
        case ST_CALCULATING:
            process_calculating(conn);
            break;
        case ST_CLEANUP:
            process_cleanup(conn);
            break;
        case ST_TIMEOUT:
            fprintf(stderr, "ERROR, process_timeout not implemented\n");
            // fall through to the next state
        case ST_ERROR:
            process_error(conn);
            break;
        default:
            fprintf(stderr, "Unknown state: %d. Internal error!\n", conn->state);
            exit(-5);
    };
};

void initialize_dispatcher()
{
    int i;
    connections = malloc(sizeof(struct connection) * config_opts.concurrency);
    if (connections == NULL) {
        fprintf(stderr, "Unable to allocate connections structure, exiting\n");
        exit(-2);
    }
    memset(connections, 0, sizeof(struct connection) * config_opts.concurrency);
    if (config_opts.verbose > 1)
        printf("Concurrency structure allocated for %d connections\n",
            config_opts.concurrency);

    /* process initial connections */
    for (i = 0; i < config_opts.concurrency; i++)
        process_state(&connections[i]);
}

