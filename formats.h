/* $Id: formats.h,v 1.3 2007/01/17 20:53:13 aaron Exp $ */
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
 * @file formats.h
 * @brief Handy-dandy string formatting routines, useful in various places.
 * @author Aaron Bannert (aaron@codemass.com)
 */

#ifndef __formats_h
#define __formats_h

#include "config.h"

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

#endif /* __formats_h */
