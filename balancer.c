/* $Id: balancer.c,v 1.12 2007/03/30 21:42:01 aaron Exp $ */
/* Copyright 2006-2007 Codemass, Inc.  All rights reserved.
 * Use is subject to license terms.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file balancer.c
 * @brief The Balancer is responsible for managing and establishing
 *        connections to all URL destinations.
 * @author Aaron Bannert (aaron@codemass.com)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

#include "params.h"
#include "balancer.h"

static struct location *locations;
static int n_locations = 0;

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
    rlen += uri->fragment ? strlen(uri->fragment) + 1 : 0;
    return rlen;
}

static size_t request_length(struct location *location)
{
    size_t rlen = 0;
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
    rlen += sizeof("\r\n\r\n") - 1;
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
    p = malloc(location->rlen) + 1;
    memset(p, 0, location->rlen + 1);
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
        const char *host;
        unsigned short port;
        struct hostent *hostent;
        locations[i].uristr = urls->url;
        locations[i].uri = parse_uri(urls->url);
        if (locations[i].uri == NULL) {
            fprintf(stderr, "error parsing URI: %s, failing\n", urls->url);
            exit(-4);
        }
        host = locations[i].uri->hostname;
        port = htons(locations[i].uri->port);
        if (config_opts.connect) {
            host = config_opts.connect;
            if (config_opts.connect_port) {
                port = htons(config_opts.connect_port);
            }
        }
         hostent = gethostbyname(host);
        if (hostent == NULL) {
            fprintf(stderr, "gethostbyname error resolving %s: %s\n",
                    host, hstrerror(h_errno));
            exit(-4);
        }
        if (hostent->h_addrtype == AF_INET) {
            struct sockaddr_in *sa = malloc(sizeof(*sa));
            locations[i].name = (struct sockaddr *)sa;
            locations[i].namelen = sizeof(*sa);
            sa->sin_family = hostent->h_addrtype;
            sa->sin_port = port;
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
    for (i = 0; i < n_locations; i++) {
        int rv = start_accumulator(&locations[i].accumulator);
        if (rv < 0) {
            perror("start_accumulator (gettimeofday()) from set_locations");
            exit(-3);
        }
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
    if (rc < 0) {
        switch (e) {
        case EINTR:
            goto retry_connect;
        case ECONNREFUSED:
        case ENETUNREACH:
            if (config_opts.verbose > 2)
                perror("connect");
            location->n_errors++;
            goto retry_connect;
        case EAGAIN: // local port exhaustion on Linux
        case EADDRNOTAVAIL: // local port exhaustion on Solaris
        case EINPROGRESS: // non-blocking socket still connecting (good error)
        default:
            break;
        };
    }
    location->n_connects++;
    return rc;
}

int balancer_display(FILE *stream)
{
    int i, ret = 0;
    for (i = 0; i < n_locations; i++)
        (void)stop_accumulator(&locations[i].accumulator);
    for (i = 0; i < n_locations; i++) {
        ret += fprintf(stream, "Statistics for URL %d: %s\n", i + 1, locations[i].uristr);
        if (n_locations == 1)
            return ret;
        ret += print_accumulator(stream, &locations[i].accumulator);
        ret += fprintf(stream, "\n");
    }
    return ret;
}

