/**
 * Main routines for the plethora tool.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <event.h>

#include "config.h"
#include "params.h"
#include "dispatcher.h"
#include "balancer.h"
#include "metrics.h"

int main(int argc, char *argv[])
{
    int rc = 0;

    event_init();

    parse_args(argc, argv);

    if (config_opts.verbose > 0)
        print_config_opts(stdout);

    initialize_balancer();
    initialize_dispatcher();

    /* it doesn't make any connections before this call */
    rc = event_dispatch();
    if (rc < 0)
        perror("event_dispatch");
    else if (config_opts.verbose > 2)
        printf("done\n");

    balancer_display(stdout);
    dispatcher_display(stdout);

    return 0;
}
