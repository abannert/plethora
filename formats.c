/**
 * (c) 2006 Codemass, Inc.
 */

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

int format_bytes(char *buf, size_t buflen, size_t _bytes)
{
    double bytes = (double)_bytes;
    return format_double_bytes(buf, buflen, bytes);
}

