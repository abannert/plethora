/* $Id: params.h,v 1.3 2007/01/17 20:55:01 aaron Exp $ */
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
 * @file params.h
 * @brief Input parameter parsing and help display routines.
 * @author Aaron Bannert (aaron@codemass.com)
 */

#ifndef __params_h
#define __params_h

#include "config.h"

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

#endif /* __params_h */
