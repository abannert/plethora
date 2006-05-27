/**
 * (c) 2006 Codemass, Inc.
 * Parts of this were taken from Apache 2.0.
 * THIS MUST BE ELABORATED UPON BEFORE DISTRIBUTION OF ANY KIND.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>

#include "params.h"
#include "balancer.h"

static struct location *locations;
static int n_locations = 0;

#define T_COLON           0x01        /* ':' */
#define T_SLASH           0x02        /* '/' */
#define T_QUESTION        0x04        /* '?' */
#define T_HASH            0x08        /* '#' */
#define T_NUL             0x80        /* '\0' */
static const unsigned char uri_delims[256] = {
    T_NUL,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,T_HASH,0,0,0,0,
    0,0,0,0,0,0,0,T_SLASH,0,0,0,0,0,0,0,0,0,0,T_COLON,0,
    0,0,0,T_QUESTION,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

#define NOTEND_SCHEME     (0xff)
#define NOTEND_HOSTINFO   (T_SLASH | T_QUESTION | T_HASH | T_NUL)
#define NOTEND_PATH       (T_QUESTION | T_HASH | T_NUL)

#define APR_URI_FTP_DEFAULT_PORT         21 /**< default FTP port */
#define APR_URI_SSH_DEFAULT_PORT         22 /**< default SSH port */
#define APR_URI_TELNET_DEFAULT_PORT      23 /**< default telnet port */
#define APR_URI_GOPHER_DEFAULT_PORT      70 /**< default Gopher port */
#define APR_URI_HTTP_DEFAULT_PORT        80 /**< default HTTP port */
#define APR_URI_POP_DEFAULT_PORT        110 /**< default POP port */
#define APR_URI_NNTP_DEFAULT_PORT       119 /**< default NNTP port */
#define APR_URI_IMAP_DEFAULT_PORT       143 /**< default IMAP port */
#define APR_URI_PROSPERO_DEFAULT_PORT   191 /**< default Prospero port */
#define APR_URI_WAIS_DEFAULT_PORT       210 /**< default WAIS port */
#define APR_URI_LDAP_DEFAULT_PORT       389 /**< default LDAP port */
#define APR_URI_HTTPS_DEFAULT_PORT      443 /**< default HTTPS port */
#define APR_URI_RTSP_DEFAULT_PORT       554 /**< default RTSP port */
#define APR_URI_SNEWS_DEFAULT_PORT      563 /**< default SNEWS port */
#define APR_URI_ACAP_DEFAULT_PORT       674 /**< default ACAP port */
#define APR_URI_NFS_DEFAULT_PORT       2049 /**< default NFS port */
#define APR_URI_TIP_DEFAULT_PORT       3372 /**< default TIP port */
#define APR_URI_SIP_DEFAULT_PORT       5060 /**< default SIP port */

struct schemes_t {
    /** The name of the scheme */
    const char *name;
    /** The default port for the scheme */
    unsigned short default_port;
};

typedef struct schemes_t schemes_t;

static schemes_t schemes[] =
{
    {"http",     APR_URI_HTTP_DEFAULT_PORT},
    {"ftp",      APR_URI_FTP_DEFAULT_PORT},
    {"https",    APR_URI_HTTPS_DEFAULT_PORT},
    {"gopher",   APR_URI_GOPHER_DEFAULT_PORT},
    {"ldap",     APR_URI_LDAP_DEFAULT_PORT},
    {"nntp",     APR_URI_NNTP_DEFAULT_PORT},
    {"snews",    APR_URI_SNEWS_DEFAULT_PORT},
    {"imap",     APR_URI_IMAP_DEFAULT_PORT},
    {"pop",      APR_URI_POP_DEFAULT_PORT},
    {"sip",      APR_URI_SIP_DEFAULT_PORT},
    {"rtsp",     APR_URI_RTSP_DEFAULT_PORT},
    {"wais",     APR_URI_WAIS_DEFAULT_PORT},
    {"z39.50r",  APR_URI_WAIS_DEFAULT_PORT},
    {"z39.50s",  APR_URI_WAIS_DEFAULT_PORT},
    {"prospero", APR_URI_PROSPERO_DEFAULT_PORT},
    {"nfs",      APR_URI_NFS_DEFAULT_PORT},
    {"tip",      APR_URI_TIP_DEFAULT_PORT},
    {"acap",     APR_URI_ACAP_DEFAULT_PORT},
    {"telnet",   APR_URI_TELNET_DEFAULT_PORT},
    {"ssh",      APR_URI_SSH_DEFAULT_PORT},
    { NULL, 0xFFFF }     /* unknown port */
};

short uri_port_of_scheme(const char *scheme_str)
{
    schemes_t *scheme;

    if (scheme_str) {
        for (scheme = schemes; scheme->name != NULL; ++scheme) {
            if (strcasecmp(scheme_str, scheme->name) == 0) {
                return scheme->default_port;
            }
        }
    }
    return 0;
}

struct uri *parse_uri(const char *uri)
{
    const char *s;
    const char *s1;
    const char *hostinfo;
    char *endstr;
    int port;
    int v6_offset1 = 0, v6_offset2 = 0;

    /* Initialize the structure. parse_uri() and parse_uri_components()
     * can be called more than once per request.
     */
    struct uri *uptr = malloc(sizeof(*uptr));
    memset(uptr, '\0', sizeof(*uptr));

    /* We assume the processor has a branch predictor like most --
     * it assumes forward branches are untaken and backwards are taken.  That's
     * the reason for the gotos.  -djg
     */
    if (uri[0] == '/') {
deal_with_path:
        /* we expect uri to point to first character of path ... remember
         * that the path could be empty -- http://foobar?query for example
         */
        s = uri;
        while ((uri_delims[*(unsigned char *)s] & NOTEND_PATH) == 0) {
            ++s;
        }
        if (s != uri) {
            uptr->path = malloc(s - uri + 1);
            memset(uptr->path, 0, s - uri + 1);
            memcpy(uptr->path, uri, s - uri);
        }
        if (*s == 0) {
            return uptr;
        }
        if (*s == '?') {
            ++s;
            s1 = strchr(s, '#');
            if (s1) {
                uptr->fragment = strdup(s1 + 1);
                uptr->query = malloc(s1 - s + 1);
                memset(uptr->query, 0, s1 - s + 1);
                memcpy(uptr->query, s, s1 - s);
            }
            else {
                uptr->query = strdup(s);
            }
            return uptr;
        }
        /* otherwise it's a fragment */
        uptr->fragment = strdup(s + 1);
        return uptr;
    }

    /* find the scheme: */
    s = uri;
    while ((uri_delims[*(unsigned char *)s] & NOTEND_SCHEME) == 0) {
        ++s;
    }
    /* scheme must be non-empty and followed by :// */
    if (s == uri || s[0] != ':' || s[1] != '/' || s[2] != '/') {
        goto deal_with_path;        /* backwards predicted taken! */
    }

    uptr->scheme = malloc(s - uri + 1);
    memset(uptr->scheme, 0, s - uri + 1);
    memcpy(uptr->scheme, uri, s - uri);
    s += 3;
    hostinfo = s;
    while ((uri_delims[*(unsigned char *)s] & NOTEND_HOSTINFO) == 0) {
        ++s;
    }
    uri = s;        /* whatever follows hostinfo is start of uri */
    uptr->hostinfo = malloc(uri - hostinfo + 1);
    memset(uptr->hostinfo, 0, uri - hostinfo + 1);
    memcpy(uptr->hostinfo, hostinfo, uri - hostinfo);

    /* If there's a username:password@host:port, the @ we want is the last @...
     * too bad there's no memrchr()... For the C purists, note that hostinfo
     * is definately not the first character of the original uri so therefore
     * &hostinfo[-1] < &hostinfo[0] ... and this loop is valid C.
     */
    do {
        --s;
    } while (s >= hostinfo && *s != '@');
    if (s < hostinfo) {
        /* again we want the common case to be fall through */
deal_with_host:
        /* We expect hostinfo to point to the first character of
         * the hostname.  If there's a port it is the first colon,
         * except with IPv6.
         */
        if (*hostinfo == '[') {
            v6_offset1 = 1;
            v6_offset2 = 2;
            s = memchr(hostinfo, ']', uri - hostinfo);
            if (s == NULL) {
                return NULL;
            }
            if (*++s != ':') {
                s = NULL; /* no port */
            }
        }
        else {
            s = memchr(hostinfo, ':', uri - hostinfo);
        }
        if (s == NULL) {
            /* we expect the common case to have no port */
            uptr->hostname = malloc(uri - hostinfo - v6_offset2 + 1);
            memset(uptr->hostname, 0, uri - hostinfo - v6_offset2 + 1);
            memcpy(uptr->hostname, hostinfo + v6_offset1,
                                   uri - hostinfo - v6_offset2);
            uptr->port = uri_port_of_scheme(uptr->scheme);
            goto deal_with_path;
        }
        uptr->hostname = malloc(s - hostinfo - v6_offset2 + 1);
        memset(uptr->hostname, 0, s - hostinfo - v6_offset2 + 1);
        memcpy(uptr->hostname, hostinfo + v6_offset1,
                               s - hostinfo - v6_offset2);
        ++s;
        uptr->port_str = malloc(uri - s + 1);
        memset(uptr->port_str, 0, uri - s + 1);
        memcpy(uptr->port_str, s, uri - s);
        if (uri != s) {
            port = strtol(uptr->port_str, &endstr, 10);
            uptr->port = port;
            if (*endstr == '\0') {
                goto deal_with_path;
            }
            /* Invalid characters after ':' found */
            return NULL;
        }
        uptr->port = uri_port_of_scheme(uptr->scheme);
        goto deal_with_path;
    }

    /* first colon delimits username:password */
    s1 = memchr(hostinfo, ':', s - hostinfo);
    if (s1) {
        uptr->user = malloc(s1 - hostinfo + 1);
        memset(uptr->user, 0, s1 - hostinfo + 1);
        memcpy(uptr->user, hostinfo, s1 - hostinfo);
        ++s1;
        uptr->password = malloc(s - s1 + 1);
        memset(uptr->password, 0, s - s1 + 1);
        memcpy(uptr->password, s1, s - s1);
    }
    else {
        uptr->user = malloc(s - hostinfo + 1);
        memset(uptr->user, 0, s - hostinfo + 1);
        memcpy(uptr->user, hostinfo, s - hostinfo);
    }
    hostinfo = s + 1;
    goto deal_with_host;
}

static int count_locations(struct urls *urls)
{
    int i = 0;
    struct urls *p = urls;
    while (p) {
        i++;
        if (p->next == urls)
            break;
        p = p->next;
    }
    return i;
}

static size_t resource_length(struct location *location)
{
    size_t rlen = 0;
    struct uri *uri = location->uri;
    rlen += uri->path ? strlen(uri->path) : sizeof("/") - 1;
    rlen += uri->query ? strlen(uri->query) + 1 : 0;
    /* FIXME: do we add fragments to client requests? */
    rlen += uri->fragment ? strlen(uri->fragment) + 1: 0;
    return rlen;
}

static size_t request_length(struct location *location)
{
    size_t rlen = 1; /* for the null */
    struct headers *header = config_opts.headers;
    rlen += sizeof("GET ") - 1;
    rlen += resource_length(location);
    rlen += sizeof(" HTTP/1.1\r\n") - 1;
    while (1) {
        if (header->value) {
            rlen += strlen(header->header);
            rlen += sizeof(": ") - 1;
            rlen += strlen(header->value);
            rlen += sizeof("\r\n") - 1;
        }
        if (header->next == config_opts.headers)
            break;
        header = header->next;
    }
    rlen += sizeof("Host: ") - 1;
    rlen += strlen(location->uri->hostname);
    rlen += sizeof("\r\n\r\n");
    return rlen;
}

static size_t write_resource(struct uri *uri, char *p)
{
    char *q = p;
    if (config_opts.verbose > 5) {
        fprintf(stderr, "uri->path = '%s'\n", uri->path ? uri->path : "NULL");
        fprintf(stderr, "uri->query = '%s'\n", uri->query ? uri->query : "NULL");
        fprintf(stderr, "uri->fragment = '%s'\n", uri->fragment ? uri->fragment : "NULL");
    }
    if (uri->path)
        p += sprintf(p, "GET %s", uri->path);
    else
        p += sprintf(p, "GET /");
    if (uri->query)
        p += sprintf(p, "?%s", uri->query);
    if (uri->fragment)
        p += sprintf(p, "#%s", uri->fragment);
    p += sprintf(p, " HTTP/1.1\r\n");
    return p - q;
}

static void create_request(struct location *location)
{
    struct headers *header = config_opts.headers;
    char *p;
    location->rlen = request_length(location);
    p = malloc(location->rlen);
    location->request = p;
    p += write_resource(location->uri, p);
    while (1) {
        if (header->value)
            p += sprintf(p, "%s: %s\r\n", header->header, header->value);
        if (header->next == config_opts.headers)
            break;
        header = header->next;
    }
    p += sprintf(p, "Host: %s\r\n\r\n", location->uri->hostname);
}

static void set_locations(struct urls *urls)
{
    int i;
    for (i = 0; i < n_locations; i++) {
        struct hostent *hostent;
        locations[i].uri = parse_uri(urls->url);
        if (locations[i].uri == NULL) {
            fprintf(stderr, "error parsing URI: %s, failing\n", urls->url);
            exit(-4);
        }
        hostent = gethostbyname(locations[i].uri->hostname);
        if (hostent == NULL) {
            fprintf(stderr, "gethostbyname error resolving %s: %s\n",
                    locations[i].uri->hostname,
                    hstrerror(h_errno));
            exit(-4);
        }
        if (hostent->h_addrtype == AF_INET) {
            struct sockaddr_in *sa = malloc(sizeof(*sa));
            locations[i].name = (struct sockaddr *)sa;
            locations[i].namelen = sizeof(*sa);
            sa->sin_family = hostent->h_addrtype;
            sa->sin_port = htons(locations[i].uri->port);
            memcpy(&(sa->sin_addr.s_addr), hostent->h_addr_list[0], hostent->h_length);
        } else if (hostent->h_addrtype == AF_INET6) {
            fprintf(stderr, "ipv6 not yet supported\n");
            exit(-3);
        } else {
            fprintf(stderr, "unknown address type\n");
            exit(-3);
        }
        create_request(&locations[i]);
        urls = urls->next;
    }
}

void initialize_balancer()
{
    n_locations = count_locations(config_opts.urls);
    locations = malloc(sizeof(struct location) * n_locations);
    memset(locations, 0, sizeof(struct location) * n_locations);
    set_locations(config_opts.urls);
}

static int current_location = -1;
struct location *get_next_location()
{
    if (++current_location >= n_locations)
        current_location = 0;
    return &locations[current_location];
}

int location_connect(struct location *location, int sock)
{
    int e;
    int rc;
    if (config_opts.verbose > 4)
        fprintf(stderr, "connect_next(%d), current_location is %d\n",
                sock, current_location);
retry_connect:
    if (location->n_errors >= config_opts.max_connect_errors) {
        fprintf(stderr, "Exceeded maximum connect errors for host: %s:%d\n",
                location->uri->hostname,
                location->uri->port);
        return -1;
    }
    if (config_opts.verbose > 3)
        fprintf(stderr, "connect()ing to socket %d\n", sock);
    rc = connect(sock, location->name, location->namelen);
    e = errno;
    if (rc < 0 && e != EINPROGRESS) {
        if (config_opts.verbose > 2)
            perror("connect");
        location->n_errors++;
        goto retry_connect;
    }
    location->n_connects++;
    return 0;
}
