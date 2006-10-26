#ifndef __metrics_h
#define __metrics_h

#include <stdio.h>
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
