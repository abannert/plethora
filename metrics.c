#include <stdio.h>
#include <errno.h>
#include <time.h>

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

struct timeval global_start_tv;
struct timeval global_end_tv;

int start_global_timer()
{
    struct timezone tz; /* ignored */
    int rv = gettimeofday(&global_start_tv, &tz);
    int e = errno;
    if (config_opts.verbose > 0)
        fprintf(stderr, "starting test at %s\n", ctime(&global_start_tv.tv_sec));
    errno = e;
    return rv;
}

int end_global_timer()
{
    struct timezone tz; /* ignored */
    int rv = gettimeofday(&global_end_tv, &tz);
    int e = errno;
    if (config_opts.verbose > 0)
        fprintf(stderr, "starting test at %s\n", ctime(&global_end_tv.tv_sec));
    errno = e;
    return rv;
}

void print_global_timing_metrics()
{
    struct timeval diff;
    double elapsed;
    timer_sub(&global_end_tv, &global_start_tv, &diff);
    elapsed = (diff.tv_sec * 1000000.0) + diff.tv_usec;
    if (elapsed < 1000.0)
        printf("Total time elapsed: %.3lfus\n", elapsed);
    else {
        elapsed /= 1000.0;
        if (elapsed < 1000.0)
            printf("Total time elapsed: %.3lfms\n", elapsed);
        else {
            elapsed /= 1000.0;
            printf("Total time elapsed: %.3lfs\n", elapsed);
        }
    }
}

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

void metrics_accumulate(struct metrics *main, struct metrics *accumulate)
{
    timer_add(&main->connect, &accumulate->connect, &main->connect);
    timer_add(&main->write, &accumulate->write, &main->write);
    timer_add(&main->read, &accumulate->read, &main->read);
    timer_add(&main->close, &accumulate->close, &main->close);
    main->total_measurements++;
}

#ifdef HAD_NO_BSD_SOURCE
#undef HAD_NO_BSD_SOURCE
#undef _BSD_SOURCE
#endif
#ifdef HAD_POSIX_SOURCE
#define _POSIX_SOURCE
#undef HAD_POSIX_SOURCE
#endif
