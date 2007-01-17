/* $Id: balancer.h,v 1.4 2007/01/17 20:51:39 aaron Exp $ */
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
 * @file balancer.h
 * @brief The Balancer is responsible for managing and establishing
 *        connections to all URL destinations.
 * @author Aaron Bannert (aaron@codemass.com)
 */

#ifndef __balancer_h
#define __balancer_h

#include "config.h"

#include <stdio.h>
#include <sys/socket.h>

#include "parse_uri.h"
#include "metrics.h"

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

#endif /* __balancer_h */
