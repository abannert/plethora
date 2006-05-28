#include <sys/socket.h>

#include "metrics.h"

struct uri {
    char *scheme;
    char *hostinfo;
    char *user;
    char *password;
    char *hostname;
    char *port_str;
    char *path;
    char *query;
    char *fragment;

    short port;
};

struct location {
    const char *uristr;
    struct uri *uri;
    struct sockaddr *name;
    socklen_t namelen;

    const char *request;
    size_t rlen;

    int n_errors;
    int n_connects;
    struct accumulator accumulator;
};

void initialize_balancer();

struct location *get_next_location();

int location_connect(struct location *location, int sock);

int balancer_display(FILE *stream);
