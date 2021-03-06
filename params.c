/* $Id: params.c,v 1.8 2007/11/28 16:55:56 aaron Exp $ */
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
 * @file params.c
 * @brief Input parameter parsing and help display implementation.
 * @author Aaron Bannert (aaron@codemass.com)
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <ctype.h>

#include "config.h"
#include "params.h"

static char *opts = "H:C:c:n:vho";

static void print_help(FILE *stream, const char *progname)
{
    fprintf(stream, "Usage: %s [options] url1 url2 ...\n", progname);
    fprintf(stream, " url1 ... - list of URLs, all to the same host\n");
    fprintf(stream, " -h - this help screen\n");
    fprintf(stream, " -H <header: value> - override, set or unset header\n");
    fprintf(stream, " -C <host> - connect to this host instead of hosts in URL\n");
    fprintf(stream, " -c <num> - concurrency level\n");
    fprintf(stream, " -n <num> - number of requests to make total\n");
    fprintf(stream, " -M <num> - maximum number of connect errors allowed, -1 to disable\n");
    fprintf(stream, " -o - half-open mode (shutdown socket for writes after sending headers)\n");
    fprintf(stream, " -v - verbose mode (add multiple times for higher verbosity)\n");
    fprintf(stream, "Hint: use \"--\" to stop argument parsing\n");
}

struct config_opts config_opts;

static struct headers default_headers[] = {
    { "User-Agent", PACKAGE_NAME "/" PACKAGE_VERSION, NULL, NULL },
    { "Accept", "*/*", NULL, NULL},
    { "Pragma", "no-cache", NULL, NULL},
    { "Connection", "close", NULL, NULL},
    { NULL, NULL, NULL, NULL },
};

static void add_default_headers()
{
    struct headers *header;
    for (header = default_headers; header->header != NULL; header++) {
        struct headers *h = malloc(sizeof(*h));
        RING_INIT(h);
        h->header = header->header;
        h->value = header->value;
        RING_APPEND(config_opts.headers, header);
    }
}

static void initialize_params()
{
    memset(&config_opts, 0, sizeof(config_opts));
    config_opts.concurrency = 1;
    config_opts.count = 1;
    config_opts.halfopen = 0;
    add_default_headers();
    config_opts.max_connect_errors = MAX_CONNECT_ERRORS;
}

static struct headers *parse_header_param(const char *optarg)
{
    struct headers *header;
    char *hp;
    char *h = optarg ? strdup(optarg) : NULL;
    if (h == NULL)
        return NULL;

    header = malloc(sizeof(*header));
    RING_INIT(header);
    memset(header, 0, sizeof(*header));

    if ((hp = strstr(h, ":")) == NULL) {
        header->header = h; // only the header was found
        return header;
    } else {
        *hp++ = '\0';
        while (*h == ' ') h++; /* consume whitespace */
        header->header = h;
        while (*hp == ' ') hp++; /* consume whitespace */
        header->value = hp;
        return header;
    }
}

/**
 * Lower-case a string in-place.
 */
#define LC(s) \
do { \
    ssize_t len = strlen(s); \
    int i; \
    for (i = 0; i < len; i++) { \
        s[i] = tolower(s[i]); \
    } \
} while (0)

/**
 * Add the given header to the header list, overwriting any previous
 * header entries having the same (case-insensitive) key.
 */
static void overwrite_header(struct headers **headers, struct headers *header)
{
    /* find any previous instance of the header (case-insensitive) and delete */
    char *newheader = strdup(header->header);
    struct headers *p = *headers;

    /* lower-case the new header */
    LC(newheader);

    while (1) {
        /* lower-case the old header */
        char *oldheader = strdup(p->header);
        LC(oldheader);

        if (strcmp(newheader, oldheader) == 0) {
            /* delete the old header key and value */
            free(p->header);
            free(p->value);
            p->header = header->header;
            p->value = header->value;
            free(header);
            return;
        }

        if (p->next == *headers)
            break;
        p = p->next;
    }
    /* no match found, append the new header to the end of the list */
    RING_APPEND((*headers), header);
}

void parse_args(int argc, char *argv[])
{
    long l;
    int i;
    const char *progname = argv[0];
    struct headers *header;

    initialize_params();

    while ((i = getopt(argc, argv, opts)) > 0) {
        switch (i) {
            case 'H':
                header = parse_header_param(optarg);
                if (header == NULL) {
                    fprintf(stderr, "error parsing -H parameter\n");
                    print_help(stderr, progname);
                    exit(-1);
                } else {
                    overwrite_header(&config_opts.headers, header);
                }
                break;
            case 'C':
                {
                    /* split on : if present
                     * first part is config_opts.connect
                     * second part is config_opts.connect_port
                     */
                    char *portstr;
                    config_opts.connect = strdup(optarg);
                    portstr = strchr(config_opts.connect, ':');
                    if (portstr)
                        *portstr++ = '\0'; /* terminate the host part */
                    if (portstr && *portstr) {
                        errno = 0;
                        l = strtol(portstr, (char **)NULL, 10);
                        if (errno) {
                            perror("invalid connect port (-C) value");
                            print_help(stderr, progname);
                            exit(-1);
                        } else if (l < 0 || l >= USHRT_MAX) {
                            fprintf(stderr, "invalid connect port (-C): %ld\n",
                                    l);
                            print_help(stderr, progname);
                            exit(-1);
                        }
                        config_opts.connect_port = (unsigned short)l;
                    }
                }
                break;
            case 'c':
                errno = 0;
                l = strtol(optarg, (char **)NULL, 10);
                if (errno) {
                    perror("invalid concurrency (-c) value");
                    print_help(stderr, progname);
                    exit(-1);
                } else if (l <= 0 || l == LONG_MAX) {
                    fprintf(stderr, "invalid concurrency (-c): %ld\n", l);
                    print_help(stderr, progname);
                    exit(-1);
                }
                config_opts.concurrency = (int)l;
                break;
            case 'n':
                errno = 0;
                l = strtol(optarg, (char **)NULL, 10);
                if (errno) {
                    perror("invalid request count (-n) value");
                    print_help(stderr, progname);
                    exit(-1);
                } else if (l <= 0 || l == LONG_MAX) {
                    fprintf(stderr, "invalid request count (-n): %ld\n", l);
                    print_help(stderr, progname);
                    exit(-1);
                }
                config_opts.count = (int)l;
                break;
            case 'o':
                config_opts.halfopen = 1;
                break;
            case 'v':
                config_opts.verbose++;
                break;
            default:
                fprintf(stderr, "Unknown option: %o\n", i);
            case 'h':
                print_help(stderr, progname);
                exit(-1);
                break;
        }
    }
    argc -= optind;
    argv += optind;
    if (argc == 0) {
        fprintf(stderr, "At least one URL required...");
        print_help(stderr, progname);
        exit(-1);
    } else for (i = 0; i < argc; i++) {
        struct urls *url = malloc(sizeof(*url));
        RING_INIT(url);
        url->url = strdup(argv[i]);
        RING_APPEND(config_opts.urls, url);
    }
}

void print_config_opts(FILE *stream)
{
    struct urls *urls = config_opts.urls;
    struct headers *headers = config_opts.headers;
    fprintf(stream, "Fetching these URLs\n");
    while (1) {
        fprintf(stream, " %s\n", urls->url);
        if (urls->next == config_opts.urls)
            break;
        urls = urls->next;
    }
    fprintf(stream, "Using these headers:\n");
    while (1) {
        if (headers->value)
            fprintf(stream, "  %s: %s\n", headers->header, headers->value);
        else
            fprintf(stream, "  %s (DISABLED)\n", headers->header);
        if (headers->next == config_opts.headers)
            break;
        headers = headers->next;
    }
    if (config_opts.connect) {
        if (config_opts.connect_port) {
            fprintf(stream, "Connecting to this host:port (-C): %s:%u\n",
                    config_opts.connect, config_opts.connect_port);
        } else {
            fprintf(stream, "Connecting to this host (-C): %s\n",
                    config_opts.connect);
        }
    }
    fprintf(stream, "Concurrency (-c): %d\n", config_opts.concurrency);
    fprintf(stream, "Total request count (-n): %d\n", config_opts.count);
    fprintf(stream, "Shutdown socket for writes after sending headers "
                    "(halfopen) (-o): %s\n",
                    config_opts.halfopen ? "true" : "false");
    fprintf(stream, "Verbosity level (-v): %d\n", config_opts.verbose);
    fprintf(stream, "\n");
}
