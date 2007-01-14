/**
 * (c) 2006 Codemass, Inc.  All Rights Reserved
 * @author Aaron Bannert (aaron@codemass.com)
 *
 * Handy-dandy string formatting routines, useful in various places.
 */

/**
 * Treat the given double as a high-precision microsecond (us) timer
 * and format it into pretty-printable units.
 */
int format_double_timer(char *buf, size_t buflen, double time_us);

/**
 * Format the given double as bytes into pretty-printable units.
 */
int format_double_bytes(char *buf, size_t buflen, double bytes);

/**
 * Format the given bytes into pretty-printable units.
 */
int format_bytes(char *buf, size_t buflen, unsigned long long bytes);
