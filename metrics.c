#ifdef _POSIX_SOURCE
#define HAD_POSIX_SOURCE
#undef _POSIX_SOURCE
#endif
#ifndef _BSD_SOURCE
#define HAD_NO_BSD_SOURCE
#define _BSD_SOURCE
#endif

#include <stdio.h>
#include <errno.h>
#include <time.h>

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
    timersub(&global_end_tv, &global_start_tv, &diff);
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
    timeradd(&main->connect, &accumulate->connect, &main->connect);
    timeradd(&main->write, &accumulate->write, &main->write);
    timeradd(&main->read, &accumulate->read, &main->read);
    timeradd(&main->close, &accumulate->close, &main->close);
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
