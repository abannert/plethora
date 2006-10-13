#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <limits.h>

#include "formats.h"

#define timer_cmp(tv1, comparator, tv2) \
    (tv1)->tv_sec == (tv2)->tv_sec ? \
    (tv1)->tv_usec comparator (tv2)->tv_usec : \
    (tv1)->tv_sec comparator (tv2)->tv_sec

#define timer_add(tv1, tv2, out) \
    do { \
        (out)->tv_sec = (tv1)->tv_sec + (tv2)->tv_sec; \
        (out)->tv_usec = (tv1)->tv_usec + (tv2)->tv_usec; \
        if ((out)->tv_usec >= 1000000) { \
            (out)->tv_sec++; \
            (out)->tv_usec -= 1000000; \
        } \
    } while (0)
#define timer_sub(tv1, tv2, out) \
    do { \
        (out)->tv_sec = (tv1)->tv_sec - (tv2)->tv_sec; \
        (out)->tv_usec = (tv1)->tv_usec - (tv2)->tv_usec; \
        if ((out)->tv_usec < 0) { \
          (out)->tv_sec--; \
          (out)->tv_usec += 1000000; \
        } \
    } while (0)

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
    acc->min.read.tv_sec = LONG_MAX;
    acc->min.read.tv_usec = LONG_MAX;
    acc->min.close.tv_sec = LONG_MAX;
    acc->min.close.tv_usec = LONG_MAX;
    return gettimeofday(&acc->start, &tz);
}

int stop_accumulator(struct accumulator *acc)
{
    struct timezone tz; /* ignored */
    int ret = gettimeofday(&acc->stop, &tz);
    if (ret < 0)
        return ret;
    timer_sub(&acc->stop, &acc->start, &acc->tdiff); // calc the tdiff
    return ret;
}

void accumulate_metrics(struct accumulator *acc, struct metrics *metrics)
{
    struct metrics mdiff;

    // increase the total number of accumulated measurements
    acc->total_measurements++;

    // calculate diff from epoch
    timer_sub(&metrics->connect, &metrics->epoch, &mdiff.connect);
    timer_sub(&metrics->write, &metrics->epoch, &mdiff.write);
    timer_sub(&metrics->read, &metrics->epoch, &mdiff.read);
    timer_sub(&metrics->close, &metrics->epoch, &mdiff.close);

    // add diff to total
    timer_add(&acc->total.connect, &mdiff.connect, &acc->total.connect);
    timer_add(&acc->total.write, &mdiff.write, &acc->total.write);
    timer_add(&acc->total.read, &mdiff.read, &acc->total.read);
    timer_add(&acc->total.close, &mdiff.close, &acc->total.close);

    // set MINs and MAXs
    if (timer_cmp(&mdiff.connect, <, &acc->min.connect)) {
        acc->min.connect = mdiff.connect;
    } else if (timer_cmp(&mdiff.connect, >, &acc->max.connect)) {
        acc->max.connect = mdiff.connect;
    }
    if (timer_cmp(&mdiff.write, <, &acc->min.write)) {
        acc->min.write = mdiff.write;
    } else if (timer_cmp(&mdiff.write, >, &acc->max.write)) {
        acc->max.write = mdiff.write;
    }
    if (timer_cmp(&mdiff.read, <, &acc->min.read)) {
        acc->min.read = mdiff.read;
    } else if (timer_cmp(&mdiff.read, >, &acc->max.read)) {
        acc->max.read = mdiff.read;
    }
    if (timer_cmp(&mdiff.close, <, &acc->min.close)) {
        acc->min.close = mdiff.close;
    } else if (timer_cmp(&mdiff.close, >, &acc->max.close)) {
        acc->max.close = mdiff.close;
    }
}

static int print_elapsed(char *buf, size_t len,
                         struct timeval *start, struct timeval *end)
{
    struct timeval diff;
    double elapsed;
    timer_sub(end, start, &diff);
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
    return 0;
}

#ifdef HAD_NO_BSD_SOURCE
#undef HAD_NO_BSD_SOURCE
#undef _BSD_SOURCE
#endif
#ifdef HAD_POSIX_SOURCE
#define _POSIX_SOURCE
#undef HAD_POSIX_SOURCE
#endif
