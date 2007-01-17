/* $Id: dispatcher.h,v 1.3 2007/01/17 20:52:48 aaron Exp $ */
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
 * @file dispatcher.h
 * @brief The Dispatcher is responsible for distributing URLs across available
 *        connections.
 * @author Aaron Bannert (aaron@codemass.com)
 */

#ifndef __dispatcher_h
#define __dispatcher_h

#include "config.h"

#include <stdio.h>

enum state {
    ST_IDLE = 0,
    ST_CONNECTING,
    ST_CONNECTED,
    ST_WRITING,
    ST_WRITTEN,
    ST_READING_HEADER,
    ST_READING_BODY,
    ST_READ,
    ST_CLOSING,
    ST_CLOSED,
    ST_CALCULATING,
    ST_CLEANUP,
    ST_TIMEOUT,
    ST_ERROR,
};

struct connection;

void initialize_dispatcher();

int dispatcher_display(FILE *stream);

#endif /* __dispatcher_h */
