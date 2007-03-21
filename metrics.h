/* $Id: metrics.h,v 1.7 2007/03/21 20:35:55 aaron Exp $ */
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
 * @file metrics.h
 * @brief Various metric arithmetic and accumulation routines and
 *        printing routines.
 * @author Aaron Bannert (aaron@codemass.com)
 */

#ifndef __metrics_h
#define __metrics_h

#include "config.h"

#include <stdio.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

/* Adapted from various sources, including BSD's sys/time.h */
#ifndef timercmp
#define timercmp(tv1, tv2, CMP) \
    (tv1)->tv_sec == (tv2)->tv_sec ? \
    (tv1)->tv_usec CMP (tv2)->tv_usec : \
    (tv1)->tv_sec CMP (tv2)->tv_sec
#endif

/* Adapted from various sources, including BSD's sys/time.h */
#ifndef timeradd
#define timeradd(tv1, tv2, out) \
    do { \
        (out)->tv_sec = (tv1)->tv_sec + (tv2)->tv_sec; \
        (out)->tv_usec = (tv1)->tv_usec + (tv2)->tv_usec; \
        if ((out)->tv_usec >= 1000000) { \
            (out)->tv_sec++; \
            (out)->tv_usec -= 1000000; \
        } \
    } while (0)
#endif

/* Adapted from various sources, including BSD's sys/time.h */
#ifndef timersub
#define timersub(tv1, tv2, out) \
    do { \
        (out)->tv_sec = (tv1)->tv_sec - (tv2)->tv_sec; \
        (out)->tv_usec = (tv1)->tv_usec - (tv2)->tv_usec; \
        if ((out)->tv_usec < 0) { \
          (out)->tv_sec--; \
          (out)->tv_usec += 1000000; \
        } \
    } while (0)
#endif

enum metric_type {
    ME_EPOCH,
    ME_CONNECT,
    ME_WRITE,
    ME_FIRST,
    ME_READ,
    ME_CLOSE,
};

struct metrics {
    struct timeval epoch;   /* when the test was started */
    struct timeval connect; /* time when connect completed */
    struct timeval write;   /* time when request write completed */
    struct timeval first;   /* time when first byte of response was read */
    struct timeval read;    /* time when response read completed */
    struct timeval close;   /* time when close completed */
};

struct accumulator {
    struct timeval start;   /* time when test was started */
    struct timeval stop;    /* time when test was completed */
    struct timeval tdiff;   /* timeval diff between stop and start
                             * (only valid after accumulator is stopped) */
    struct metrics total;
    struct metrics min;
    struct metrics max;
    int total_measurements;
};

int start_accumulator(struct accumulator *acc);
int stop_accumulator(struct accumulator *acc);

/**
 * Take a timer measurement.
 * @returns same errors as gettimeofday(2)
 */
int measure(enum metric_type type, struct metrics *metrics);

/**
 * Add the metrics to the accumulator.
 */
void accumulate_metrics(struct accumulator *acc, struct metrics *metrics);

int print_accumulator(FILE *stream, struct accumulator *acc);

int print_metrics(FILE *stream, struct metrics *metrics);

#endif /* __metrics_h */
