#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

#include "config.h"
#include "params.h"

static char *opts = "H:C:c:n:vh";

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
    fprintf(stream, " -v - verbose mode (add multiple times for higher verbosity)\n");
    fprintf(stream, "Hint: use \"--\" to stop argument parsing\n");
}

struct config_opts config_opts;

static struct headers default_headers[] = {
    { "User-Agent", PACKAGE_NAME "/" PACKAGE_VERSION, NULL, NULL },
    { "Accept", "*/*", NULL, NULL},
    { "Pragma", "no-cache", NULL, NULL},
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
                }
                /* FIXME: overwrite pre-existing headers */
                RING_APPEND(config_opts.headers, header);
                break;
            case 'C':
                config_opts.connect = strdup(optarg);
                break;
            case 'c':
                errno = 0;
                l = strtol(optarg, (char **)NULL, 10);
                if (errno) {
                    perror("invalid concurrency (-c) value");
                    print_help(stderr, progname);
                    exit(-1);
                } else if (l <= 0 || l == INT_MAX) {
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
                } else if (l <= 0 || l == INT_MAX) {
                    fprintf(stderr, "invalid request count (-n): %ld\n", l);
                    print_help(stderr, progname);
                    exit(-1);
                }
                config_opts.count = (int)l;
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
    if (config_opts.connect)
        fprintf(stream, "Connecting to this host: %s\n", config_opts.connect);
    fprintf(stream, "Concurrency (-c): %d\n", config_opts.concurrency);
    fprintf(stream, "Total request count (-n): %d\n", config_opts.count);
    fprintf(stream, "Verbosity level (-v): %d\n", config_opts.verbose);
    fprintf(stream, "\n");
}
