/* $Id: plethora.c,v 1.3 2007/01/17 20:55:48 aaron Exp $ */
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
 * @file plethora.c
 * @brief Main entry point for the plethora tool.
 * @author Aaron Bannert (aaron@codemass.com)
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <event.h>

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
