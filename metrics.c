/* $Id: metrics.c,v 1.9 2008/04/17 16:23:13 aaron Exp $ */
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
 * @file metrics.c
 * @brief Implementations of various metric arithmetic and accumulation
 *        routines and printing routines.
 * @author Aaron Bannert (aaron@codemass.com)
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <limits.h>

#include "formats.h"
#include "params.h"
#include "metrics.h"

int measure(enum metric_type type, struct metrics *metrics)
{
    struct timeval *tv;
    struct timezone tz; /* ignored */
    switch (type) {
        case ME_EPOCH:
            tv = &metrics->epoch; break;
        case ME_CONNECT:
            tv = &metrics->connect; break;
        case ME_WRITE:
            tv = &metrics->write; break;
        case ME_FIRST:
            tv = &metrics->first; break;
        case ME_READ:
            tv = &metrics->read; break;
        case ME_CLOSE:
            tv = &metrics->close; break;
    };
    return gettimeofday(tv, &tz);
}

int start_accumulator(struct accumulator *acc)
{
    struct timezone tz; /* ignored */
    memset(acc, 0, sizeof(*acc));
    acc->min.connect.tv_sec = LONG_MAX;
    acc->min.connect.tv_usec = LONG_MAX;
    acc->min.write.tv_sec = LONG_MAX;
    acc->min.write.tv_usec = LONG_MAX;
    acc->min.first.tv_sec = LONG_MAX;
    acc->min.first.tv_usec = LONG_MAX;
    acc->min.read.tv_sec = LONG_MAX;
    acc->min.read.tv_usec = LONG_MAX;
    acc->min.close.tv_sec = LONG_MAX;
    acc->min.close.tv_usec = LONG_MAX;
    return gettimeofday(&acc->start, &tz);
}

int stop_accumulator(struct accumulator *acc)
{
    struct timezone tz; /* ignored */
    int ret;
    if (acc->complete)
        return -1; /* already stopped */
    ret = gettimeofday(&acc->stop, &tz);
    if (ret < 0)
        return ret;
    timersub(&acc->stop, &acc->start, &acc->tdiff); // calc the tdiff
    acc->complete = 1;
    return ret;
}

void accumulate_metrics(struct accumulator *acc, struct metrics *metrics)
{
    struct metrics mdiff;

    // increase the total number of accumulated measurements
    acc->total_measurements++;

    // calculate diff from epoch
    timersub(&metrics->connect, &metrics->epoch, &mdiff.connect);
    timersub(&metrics->write, &metrics->epoch, &mdiff.write);
    timersub(&metrics->first, &metrics->epoch, &mdiff.first);
    timersub(&metrics->read, &metrics->epoch, &mdiff.read);
    timersub(&metrics->close, &metrics->epoch, &mdiff.close);

    // add diff to total
    timeradd(&acc->total.connect, &mdiff.connect, &acc->total.connect);
    timeradd(&acc->total.write, &mdiff.write, &acc->total.write);
    timeradd(&acc->total.first, &mdiff.first, &acc->total.first);
    timeradd(&acc->total.read, &mdiff.read, &acc->total.read);
    timeradd(&acc->total.close, &mdiff.close, &acc->total.close);

    // set MINs and MAXs
    if (timercmp(&mdiff.connect, &acc->min.connect, <)) {
        acc->min.connect = mdiff.connect;
    } else if (timercmp(&mdiff.connect, &acc->max.connect, >)) {
        acc->max.connect = mdiff.connect;
    }
    if (timercmp(&mdiff.write, &acc->min.write, <)) {
        acc->min.write = mdiff.write;
    } else if (timercmp(&mdiff.write, &acc->max.write, >)) {
        acc->max.write = mdiff.write;
    }
    if (timercmp(&mdiff.first, &acc->min.first, <)) {
        acc->min.first = mdiff.first;
    } else if (timercmp(&mdiff.first, &acc->max.first, >)) {
        acc->max.first = mdiff.first;
    }
    if (timercmp(&mdiff.read, &acc->min.read, <)) {
        acc->min.read = mdiff.read;
    } else if (timercmp(&mdiff.read, &acc->max.read, >)) {
        acc->max.read = mdiff.read;
    }
    if (timercmp(&mdiff.close, &acc->min.close, <)) {
        acc->min.close = mdiff.close;
    } else if (timercmp(&mdiff.close, &acc->max.close, >)) {
        acc->max.close = mdiff.close;
    }
}

static int print_elapsed(char *buf, size_t len,
                         struct timeval *start, struct timeval *end)
{
    struct timeval diff;
    double elapsed;
    timersub(end, start, &diff);
    elapsed = (diff.tv_sec * 1000000.0) + diff.tv_usec;
    if (elapsed < 1000.0)
        return snprintf(buf, len, "%.3lfus\n", elapsed);
    else {
        elapsed /= 1000.0;
        if (elapsed < 1000.0)
            return snprintf(buf, len, "%.3lfms\n", elapsed);
        else {
            elapsed /= 1000.0;
            return snprintf(buf, len, "%.3lfs\n", elapsed);
        }
    }
}

#define PRINT_STATS(stream, acc, WHICH) \
    (void)format_double_timer(mean, sizeof(mean), \
                              ((acc->total.WHICH.tv_sec * 1000000.0) \
                               + acc->total.WHICH.tv_usec) \
                              / (double)acc->total_measurements); \
    (void)format_double_timer(min, sizeof(min), \
                              (acc->min.WHICH.tv_sec * 1000000.0) \
                              + acc->min.WHICH.tv_usec); \
    (void)format_double_timer(max, sizeof(max), \
                              (acc->max.WHICH.tv_sec * 1000000.0) \
                              + acc->max.WHICH.tv_usec); \
    (void)format_double_timer(total, sizeof(total), \
                              (acc->total.WHICH.tv_sec * 1000000.0) \
                              + acc->total.WHICH.tv_usec); \
    if (sizeof(#WHICH) - 1 <= 5) \
        i += fprintf(stream, "          " #WHICH "\t\t%s\t%s/%s\t%s\n", \
                     mean, min, max, total); \
    else \
        i += fprintf(stream, "          " #WHICH "\t%s\t%s/%s\t%s\n", \
                     mean, min, max, total)

int print_accumulator(FILE *stream, struct accumulator *acc)
{
    int i = 0;
    char mean[BUFSIZ], min[BUFSIZ], max[BUFSIZ], total[BUFSIZ];
    // print the connect metrics, mode, max/min
    i += fprintf(stream, " Metrics:  type\t\t mean\t\t   min/max\t\t total\n");
    PRINT_STATS(stream, acc, connect);
    PRINT_STATS(stream, acc, write);
    PRINT_STATS(stream, acc, first);
    PRINT_STATS(stream, acc, read);
    PRINT_STATS(stream, acc, close);
    (void)format_double_timer(total, sizeof(total),
                              acc->tdiff.tv_sec * 1000000.0
                              + acc->tdiff.tv_usec);
    i += fprintf(stream, " %d requests total in %s, %.3lf Requests/Second\n",
                 acc->total_measurements, total,
                 ((double)acc->total_measurements
                  / (acc->tdiff.tv_sec + (acc->tdiff.tv_usec / 1000000.0))));
    return i;
}

int print_metrics(FILE *stream, struct metrics *metrics)
{
    char cobuf[100], wbuf[100], fbuf[100], rbuf[100], clbuf[100];
    struct timeval co, w, f, r, cl;

    /* FIXME: print header lines every once in awhile */

    timersub(&metrics->connect, &metrics->epoch, &co);
    timersub(&metrics->write, &metrics->epoch, &w);
    timersub(&metrics->first, &metrics->epoch, &f);
    timersub(&metrics->read, &metrics->epoch, &r);
    timersub(&metrics->close, &metrics->epoch, &cl);

    (void)format_double_timeval(cobuf, sizeof(cobuf), &co);
    (void)format_double_timeval(wbuf, sizeof(wbuf), &w);
    (void)format_double_timeval(fbuf, sizeof(fbuf), &f);
    (void)format_double_timeval(rbuf, sizeof(rbuf), &r);
    (void)format_double_timeval(clbuf, sizeof(clbuf), &cl);

    return fprintf(stream, "%s\t%s\t%s\t%s\t%s\n",
                   cobuf, wbuf, fbuf, rbuf, clbuf);
}
