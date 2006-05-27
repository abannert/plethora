#ifndef __metrics_h
#define __metrics_h

#include <sys/time.h>

enum metric_type {
    ME_EPOCH,
    ME_CONNECT,
    ME_WRITE,
    ME_READ,
    ME_CLOSE,
};

struct metrics {
    struct timeval epoch;   /* when the test was started */
    struct timeval connect; /* time when connect completed */
    struct timeval write;   /* time when request write completed */
    struct timeval read;    /* time when response read completed */
    struct timeval close;   /* time when close completed */
    int total_measurements;
};

int start_global_timer();
int end_global_timer();

/**
 * Take a timer measurement.
 * @returns same errors as gettimeofday(2)
 */
int measure(enum metric_type type, struct metrics *metrics);

/**
 * Add one metrics structure to another.
 */
void metrics_accumulate(struct metrics *main, struct metrics *accumulate);

void print_global_timing_metrics();

#endif /* __metrics_h */
