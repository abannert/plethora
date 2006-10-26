/**
 * This was modified from Apache 2.0 (to make it work without APR's pools).
 * FILL IN THE APPROPRIATE LICENSE TERMS BEFORE ANY DISTRIBUTION HAPPENS
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
 *
 */

#include <stdlib.h>
#include <string.h>

#include "parse_uri.h"

#define T_COLON           0x01        /* ':' */
#define T_SLASH           0x02        /* '/' */
#define T_QUESTION        0x04        /* '?' */
#define T_HASH            0x08        /* '#' */
#define T_NUL             0x80        /* '\0' */
static const unsigned char uri_delims[256] = {
    T_NUL,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,T_HASH,0,0,0,0,
    0,0,0,0,0,0,0,T_SLASH,0,0,0,0,0,0,0,0,0,0,T_COLON,0,
    0,0,0,T_QUESTION,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

#define NOTEND_SCHEME     (0xff)
#define NOTEND_HOSTINFO   (T_SLASH | T_QUESTION | T_HASH | T_NUL)
#define NOTEND_PATH       (T_QUESTION | T_HASH | T_NUL)

#define APR_URI_FTP_DEFAULT_PORT         21 /**< default FTP port */
#define APR_URI_SSH_DEFAULT_PORT         22 /**< default SSH port */
#define APR_URI_TELNET_DEFAULT_PORT      23 /**< default telnet port */
#define APR_URI_GOPHER_DEFAULT_PORT      70 /**< default Gopher port */
#define APR_URI_HTTP_DEFAULT_PORT        80 /**< default HTTP port */
#define APR_URI_POP_DEFAULT_PORT        110 /**< default POP port */
#define APR_URI_NNTP_DEFAULT_PORT       119 /**< default NNTP port */
#define APR_URI_IMAP_DEFAULT_PORT       143 /**< default IMAP port */
#define APR_URI_PROSPERO_DEFAULT_PORT   191 /**< default Prospero port */
#define APR_URI_WAIS_DEFAULT_PORT       210 /**< default WAIS port */
#define APR_URI_LDAP_DEFAULT_PORT       389 /**< default LDAP port */
#define APR_URI_HTTPS_DEFAULT_PORT      443 /**< default HTTPS port */
#define APR_URI_RTSP_DEFAULT_PORT       554 /**< default RTSP port */
#define APR_URI_SNEWS_DEFAULT_PORT      563 /**< default SNEWS port */
#define APR_URI_ACAP_DEFAULT_PORT       674 /**< default ACAP port */
#define APR_URI_NFS_DEFAULT_PORT       2049 /**< default NFS port */
#define APR_URI_TIP_DEFAULT_PORT       3372 /**< default TIP port */
#define APR_URI_SIP_DEFAULT_PORT       5060 /**< default SIP port */

struct schemes_t {
    /** The name of the scheme */
    const char *name;
    /** The default port for the scheme */
    unsigned short default_port;
};

typedef struct schemes_t schemes_t;

static schemes_t schemes[] =
{
    {"http",     APR_URI_HTTP_DEFAULT_PORT},
    {"ftp",      APR_URI_FTP_DEFAULT_PORT},
    {"https",    APR_URI_HTTPS_DEFAULT_PORT},
    {"gopher",   APR_URI_GOPHER_DEFAULT_PORT},
    {"ldap",     APR_URI_LDAP_DEFAULT_PORT},
    {"nntp",     APR_URI_NNTP_DEFAULT_PORT},
    {"snews",    APR_URI_SNEWS_DEFAULT_PORT},
    {"imap",     APR_URI_IMAP_DEFAULT_PORT},
    {"pop",      APR_URI_POP_DEFAULT_PORT},
    {"sip",      APR_URI_SIP_DEFAULT_PORT},
    {"rtsp",     APR_URI_RTSP_DEFAULT_PORT},
    {"wais",     APR_URI_WAIS_DEFAULT_PORT},
    {"z39.50r",  APR_URI_WAIS_DEFAULT_PORT},
    {"z39.50s",  APR_URI_WAIS_DEFAULT_PORT},
    {"prospero", APR_URI_PROSPERO_DEFAULT_PORT},
    {"nfs",      APR_URI_NFS_DEFAULT_PORT},
    {"tip",      APR_URI_TIP_DEFAULT_PORT},
    {"acap",     APR_URI_ACAP_DEFAULT_PORT},
    {"telnet",   APR_URI_TELNET_DEFAULT_PORT},
    {"ssh",      APR_URI_SSH_DEFAULT_PORT},
    { NULL, 0xFFFF }     /* unknown port */
};

static short uri_port_of_scheme(const char *scheme_str)
{
    schemes_t *scheme;

    if (scheme_str) {
        for (scheme = schemes; scheme->name != NULL; ++scheme) {
            if (strcasecmp(scheme_str, scheme->name) == 0) {
                return scheme->default_port;
            }
        }
    }
    return 0;
}

struct uri *parse_uri(const char *uri)
{
    const char *s;
    const char *s1;
    const char *hostinfo;
    char *endstr;
    int port;
    int v6_offset1 = 0, v6_offset2 = 0;

    /* Initialize the structure. parse_uri() and parse_uri_components()
     * can be called more than once per request.
     */
    struct uri *uptr = malloc(sizeof(*uptr));
    memset(uptr, '\0', sizeof(*uptr));

    /* We assume the processor has a branch predictor like most --
     * it assumes forward branches are untaken and backwards are taken.  That's
     * the reason for the gotos.  -djg
     */
    if (uri[0] == '/') {
deal_with_path:
        /* we expect uri to point to first character of path ... remember
         * that the path could be empty -- http://foobar?query for example
         */
        s = uri;
        while ((uri_delims[*(unsigned char *)s] & NOTEND_PATH) == 0) {
            ++s;
        }
        if (s != uri) {
            uptr->path = malloc(s - uri + 1);
            memset(uptr->path, 0, s - uri + 1);
            memcpy(uptr->path, uri, s - uri);
        }
        if (*s == 0) {
            return uptr;
        }
        if (*s == '?') {
            ++s;
            s1 = strchr(s, '#');
            if (s1) {
                uptr->fragment = strdup(s1 + 1);
                uptr->query = malloc(s1 - s + 1);
                memset(uptr->query, 0, s1 - s + 1);
                memcpy(uptr->query, s, s1 - s);
            }
            else {
                uptr->query = strdup(s);
            }
            return uptr;
        }
        /* otherwise it's a fragment */
        uptr->fragment = strdup(s + 1);
        return uptr;
    }

    /* find the scheme: */
    s = uri;
    while ((uri_delims[*(unsigned char *)s] & NOTEND_SCHEME) == 0) {
        ++s;
    }
    /* scheme must be non-empty and followed by :// */
    if (s == uri || s[0] != ':' || s[1] != '/' || s[2] != '/') {
        goto deal_with_path;        /* backwards predicted taken! */
    }

    uptr->scheme = malloc(s - uri + 1);
    memset(uptr->scheme, 0, s - uri + 1);
    memcpy(uptr->scheme, uri, s - uri);
    s += 3;
    hostinfo = s;
    while ((uri_delims[*(unsigned char *)s] & NOTEND_HOSTINFO) == 0) {
        ++s;
    }
    uri = s;        /* whatever follows hostinfo is start of uri */
    uptr->hostinfo = malloc(uri - hostinfo + 1);
    memset(uptr->hostinfo, 0, uri - hostinfo + 1);
    memcpy(uptr->hostinfo, hostinfo, uri - hostinfo);

    /* If there's a username:password@host:port, the @ we want is the last @...
     * too bad there's no memrchr()... For the C purists, note that hostinfo
     * is definately not the first character of the original uri so therefore
     * &hostinfo[-1] < &hostinfo[0] ... and this loop is valid C.
     */
    do {
        --s;
    } while (s >= hostinfo && *s != '@');
    if (s < hostinfo) {
        /* again we want the common case to be fall through */
deal_with_host:
        /* We expect hostinfo to point to the first character of
         * the hostname.  If there's a port it is the first colon,
         * except with IPv6.
         */
        if (*hostinfo == '[') {
            v6_offset1 = 1;
            v6_offset2 = 2;
            s = memchr(hostinfo, ']', uri - hostinfo);
            if (s == NULL) {
                return NULL;
            }
            if (*++s != ':') {
                s = NULL; /* no port */
            }
        }
        else {
            s = memchr(hostinfo, ':', uri - hostinfo);
        }
        if (s == NULL) {
            /* we expect the common case to have no port */
            uptr->hostname = malloc(uri - hostinfo - v6_offset2 + 1);
            memset(uptr->hostname, 0, uri - hostinfo - v6_offset2 + 1);
            memcpy(uptr->hostname, hostinfo + v6_offset1,
                                   uri - hostinfo - v6_offset2);
            uptr->port = uri_port_of_scheme(uptr->scheme);
            goto deal_with_path;
        }
        uptr->hostname = malloc(s - hostinfo - v6_offset2 + 1);
        memset(uptr->hostname, 0, s - hostinfo - v6_offset2 + 1);
        memcpy(uptr->hostname, hostinfo + v6_offset1,
                               s - hostinfo - v6_offset2);
        ++s;
        uptr->port_str = malloc(uri - s + 1);
        memset(uptr->port_str, 0, uri - s + 1);
        memcpy(uptr->port_str, s, uri - s);
        if (uri != s) {
            port = strtol(uptr->port_str, &endstr, 10);
            uptr->port = port;
            if (*endstr == '\0') {
                goto deal_with_path;
            }
            /* Invalid characters after ':' found */
            return NULL;
        }
        uptr->port = uri_port_of_scheme(uptr->scheme);
        goto deal_with_path;
    }

    /* first colon delimits username:password */
    s1 = memchr(hostinfo, ':', s - hostinfo);
    if (s1) {
        uptr->user = malloc(s1 - hostinfo + 1);
        memset(uptr->user, 0, s1 - hostinfo + 1);
        memcpy(uptr->user, hostinfo, s1 - hostinfo);
        ++s1;
        uptr->password = malloc(s - s1 + 1);
        memset(uptr->password, 0, s - s1 + 1);
        memcpy(uptr->password, s1, s - s1);
    }
    else {
        uptr->user = malloc(s - hostinfo + 1);
        memset(uptr->user, 0, s - hostinfo + 1);
        memcpy(uptr->user, hostinfo, s - hostinfo);
    }
    hostinfo = s + 1;
    goto deal_with_host;
}
