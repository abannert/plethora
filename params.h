#include <stdio.h>

#include "ring.h"

#define MAX_CONNECT_ERRORS 10

struct urls {
    char *url;
    RING_T(struct urls);
};

struct headers {
    char *header;
    char *value;
    RING_T(struct headers);
};

struct config_opts {
    struct urls *urls;
    struct headers *headers;
    char *connect;
    int verbose;
    int concurrency;
    int count;
    int max_connect_errors;
};

extern struct config_opts config_opts;

void parse_args(int argc, char *argv[]);

void print_config_opts(FILE *stream);
