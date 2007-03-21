/* $Id: formats.c,v 1.4 2007/03/21 16:49:23 aaron Exp $ */
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
 * @file formats.c
 * @brief Handy-dandy string formatting routines, useful in various places.
 * @author Aaron Bannert (aaron@codemass.com)
 */

#include "formats.h"

#include <stdio.h>

int format_double_timer(char *buf, size_t buflen, double time_us)
{   
    // start out with us
    if (time_us < 1000.0)
        return snprintf(buf, buflen, "%6.2lfus", time_us);
    time_us /= 1000.0; // change to ms
    if (time_us < 1000.0)
        return snprintf(buf, buflen, "%6.2lfms", time_us);
    time_us /= 1000.0; // change to seconds
    return snprintf(buf, buflen, "%6.2lfs", time_us);
}

int format_double_timeval(char *buf, size_t buflen, struct timeval *tv)
{
    double dtv = tv->tv_sec * 1000000.0 + tv->tv_usec;
    return format_double_timer(buf, buflen, dtv);
}


int format_double_bytes(char *buf, size_t buflen, double bytes)
{
    if (bytes < 1024)
        return snprintf(buf, buflen, "%.3fB", bytes);
    bytes /= 1024;
    if (bytes < 1024)
        return snprintf(buf, buflen, "%.3fkB", bytes);
    bytes /= 1024;
    if (bytes < 1024)
        return snprintf(buf, buflen, "%.3fMB", bytes);
    bytes /= 1024;
    return snprintf(buf, buflen, "%.3fGB", bytes);
}

int format_bytes(char *buf, size_t buflen, unsigned long long _bytes)
{
    double bytes = (double)_bytes;
    if (_bytes < 1024)
        return snprintf(buf, buflen, "%lluB", _bytes);
    return format_double_bytes(buf, buflen, bytes);
}

