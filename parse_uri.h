/* $Id: parse_uri.h,v 1.2 2007/01/17 20:55:48 aaron Exp $ */
/* Copyright 2000-2005 The Apache Software Foundation or its licensors, as
 * applicable.
 * Copyright 2006-2007 Codemass, Inc.  All rights reserved.
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
 * @file parse_uri.h
 * @brief URI parse routines adapted from APR to work without pools.
 * @author Aaron Bannert (aaron@codemass.com)
 */

#ifndef __parse_uri_h
#define __parse_uri_h

struct uri {
    char *scheme;
    char *hostinfo;
    char *user;
    char *password;
    char *hostname;
    char *port_str;
    char *path;
    char *query;
    char *fragment;

    short port;
};

struct uri *parse_uri(const char *uri);

#endif /* __parse_uri_h */
