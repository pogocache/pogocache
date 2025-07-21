// https://github.com/tidwall/pogocache
//
// Copyright 2025 Polypoint Labs, LLC. All rights reserved.
// This file is part of the Pogocache project.
// Use of this source code is governed by the AGPL that can be found in
// the LICENSE file.
//
// For alternative licensing options or general questions, please contact
// us at licensing@polypointlabs.com.
//
// Unit http.c provides the parser for the HTTP wire protocol.
#define _GNU_SOURCE  
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include "stats.h"
#include "util.h"
#include "parse.h"

extern const bool useauth;
extern const char *auth;

bool http_valid_key(const char *key, size_t len) {
    if (len == 0 || len > 250) {
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        if (key[i] <= ' ' || key[i] >= 0x7F || key[i] == '%' || key[i] == '+' ||
            key[i] == '@' || key[i] == '$' || key[i] == '?' || key[i] == '=') 
        {
            return false;
        }
    }
    return true;
}

ssize_t parse_http(const char *data, size_t len, struct args *args, 
    int *httpvers, bool *keepalive)
{
    *keepalive = false;
    *httpvers = 0;
    const char *method = 0;
    size_t methodlen = 0;
    const char *uri = 0;
    size_t urilen = 0;
    int proto = 0;
    const char *hdrname = 0; 
    size_t hdrnamelen = 0;
    const char *hdrval = 0;
    size_t hdrvallen = 0;
    size_t bodylen = 0;
    bool nocontentlength = true;
    bool html = false;
    const char *authhdr = 0;
    size_t authhdrlen = 0;
    const char *p = data;
    const char *e = p+len;
    const char *s = p;
    while (p < e) {
        if (*p == ' ') {
            method = s;
            methodlen = p-s;
            p++;
            break;
        }
        if (*p == '\n') {
            goto badreq;
        }
        p++;
    }
    s = p;
    while (p < e) {
        if (*p == ' ') {
            uri = s;
            urilen = p-s;
            p++;
            break;
        }
        if (*p == '\n') {
            goto badreq;
        }
        p++;
    }
    s = p;
    while (p < e) {
        if (*p == '\n') {
            if (*(p-1) != '\r') {
                goto badreq;
            }
            if (p-s-1 != 8 || !bytes_const_eq(s, 5, "HTTP/") || 
                s[5] < '0' || s[5] > '9' || s[6] != '.' || 
                s[7] < '0' || s[7] > '9')
            {
                goto badproto;
            }
            proto = (s[5]-'0')*10+(s[7]-'0');
            if (proto < 9 || proto >= 30) {
                goto badproto;
            }
            if (proto >= 11) {
                *keepalive = true;
            }
            *httpvers = proto;
            p++;
            goto readhdrs;
        }
        
        p++;
    }
    goto badreq;
readhdrs:
    // Parse the headers, pulling the pairs along the way.
    while (p < e) {
        hdrname = p;
        while (p < e) {
            if (*p == ':') {
                hdrnamelen = p-hdrname;
                p++;
                while (p < e && *p == ' ') {
                    p++;
                }
                hdrval = p;
                while (p < e) {
                    if (*p == '\n') {
                        if (*(p-1) != '\r') {
                            goto badreq;
                        }
                        hdrvallen = p-hdrval-1;
                        // printf("[%.*s]=[%.*s]\n", (int)hdrnamelen, hdrname,
                        //     (int)hdrvallen, hdrval);
                        // We have a new header pair (hdrname, hdrval);
                        if (argeq_bytes(hdrname, hdrnamelen, "content-length")){
                            uint64_t x;
                            if (!parse_u64(hdrval, hdrvallen, &x) || 
                                x > MAXARGSZ)
                            {
                                stat_store_too_large_incr(0);
                                goto badreq;
                            }
                            bodylen = x;
                            nocontentlength = false;
                        } else if (argeq_bytes(hdrname, hdrnamelen,
                            "connection"))
                        {
                            *keepalive = argeq_bytes(hdrval, hdrvallen, 
                                "keep-alive");
                        } else if (argeq_bytes(hdrname, hdrnamelen,
                            "accept"))
                        {
                            if (memmem(hdrval, hdrvallen, "text/html", 9) != 0){
                                html = true;
                            }
                        } else if (argeq_bytes(hdrname, hdrnamelen,
                            "authorization"))
                        {
                            authhdr = hdrval;
                            authhdrlen = hdrvallen;
                        }
                        p++;
                        if (p < e && *p == '\r') {
                            p++;
                            if (p < e && *p == '\n') {
                                p++;
                            } else {
                                goto badreq;
                            }
                            goto readbody;
                        }
                        break;
                    }
                    p++;
                }
                break;
            }
            p++;
        }
    }
    return 0;
readbody:
    // read the content body
    if ((size_t)(e-p) < bodylen) {
        return 0;
    }
    const char *body = p;
    p = e;

    // check
    if (urilen == 0 || uri[0] != '/') {
        goto badreq;
    }
    uri++;
    urilen--;
    const char *ex = 0;
    size_t exlen = 0;
    const char *flags = 0;
    size_t flagslen = 0;
    const char *cas = 0;
    size_t caslen = 0;
    const char *qauth = 0;
    size_t qauthlen = 0;
    bool xx = false;
    bool nx = false;
    // Parse the query string, pulling the pairs along the way.
    size_t querylen = 0;
    const char *query = memchr(uri, '?', urilen);
    if (query) {
        querylen = urilen-(query-uri);
        urilen = query-uri;
        query++;
        querylen--;
        const char *qkey;
        size_t qkeylen;
        const char *qval;
        size_t qvallen;
        size_t j = 0;
        size_t k = 0;
        for (size_t i = 0; i < querylen; i++) {
            if (query[i] == '=') {
                k = i;
                i++;
                for (; i < querylen; i++) {
                    if (query[i] == '&') {
                        break;
                    }
                }
                qval = query+k+1;
                qvallen = i-k-1;
            qkeyonly:
                qkey = query+j;
                qkeylen = k-j;
                // We have a new query pair (qkey, qval);
                if (bytes_const_eq(qkey, qkeylen, "flags")) {
                    flags = qval;
                    flagslen = qvallen;
                } else if (bytes_const_eq(qkey, qkeylen, "ex") || 
                    bytes_const_eq(qkey, qkeylen, "ttl"))
                {
                    ex = qval;
                    exlen = qvallen;
                } else if (bytes_const_eq(qkey, qkeylen, "cas")) {
                    cas = qval;
                    caslen = qvallen;
                } else if (bytes_const_eq(qkey, qkeylen, "xx")) {
                    xx = true;
                } else if (bytes_const_eq(qkey, qkeylen, "nx")) {
                    nx = true;
                } else if (bytes_const_eq(qkey, qkeylen, "auth")) {
                    qauth = qval;
                    qauthlen = qvallen;
                }
                j = i+1;
            } else if (query[i] == '&' || i == querylen-1) {
                qval = 0;
                qvallen = 0;
                if (i == querylen-1) {
                    i++;
                }
                k = i;
                goto qkeyonly;
            }
        }
    }
    // The entire HTTP request is complete.
    // Turn request into valid command arguments.
    if (bytes_const_eq(method, methodlen, "GET")) {
        if (urilen > 0 && uri[0] == '@') {
            // system command such as @stats or @flushall
            goto badreq;
        } else if (urilen == 0) {
            goto showhelp;
        } else {
            if (!http_valid_key(uri, urilen)) {
                goto badkey;
            }
            args_append(args, "get", 3, true);
            args_append(args, uri, urilen, true);
        }
    } else if (bytes_const_eq(method, methodlen, "PUT")) {
        if (nocontentlength) {
            // goto badreq;
        }
        if (urilen > 0 && uri[0] == '@') {
            goto badreq;
        }
        if (!http_valid_key(uri, urilen)) {
            goto badkey;
        }
        args_append(args, "set", 3, true);
        args_append(args, uri, urilen, true);
        args_append(args, body, bodylen, true);
        if (cas) {
            args_append(args, "cas", 3, true);
            args_append(args, cas, caslen, true);
        }
        if (ex) {
            args_append(args, "ex", 2, true);
            args_append(args, ex, exlen, true);
        }
        if (flags) {
            args_append(args, "flags", 5, true);
            args_append(args, flags, flagslen, true);
        }
        if (xx) {
            args_append(args, "xx", 2, true);
        }
        if (nx) {
            args_append(args, "nx", 2, true);
        }
    } else if (bytes_const_eq(method, methodlen, "DELETE")) {
        if (urilen > 0 && uri[0] == '@') {
            goto badreq;
        }
        if (!http_valid_key(uri, urilen)) {
            goto badkey;
        }
        args_append(args, "del", 3, true);
        args_append(args, uri, urilen, true);
    } else {
        parse_seterror("Method Not Allowed");
        goto badreq;
    }

    // Check authorization
    const char *authval = 0;
    size_t authvallen = 0;
    if (qauthlen > 0) {
        authval = qauth;
        authvallen = qauthlen;
    } else if (authhdrlen > 0) {
        if (authhdrlen >= 7 && strncmp(authhdr, "Bearer ", 7) == 0) {
            authval = authhdr + 7;
            authvallen = authhdrlen - 7;
        } else {
            goto unauthorized;
        }
    }
    if (useauth || authvallen > 0) {
        stat_auth_cmds_incr(0);
        size_t authlen = strlen(auth);
        if (authvallen != authlen || memcmp(auth, authval, authlen) != 0) {
            stat_auth_errors_incr(0);
            goto unauthorized;
        }

    }
    return e-data;
badreq:
    parse_seterror("Bad Request");
    return -1;
badproto:
    parse_seterror("Bad Request");
    return -1;
badkey:
    parse_seterror("Invalid Key");
    return -1;
unauthorized:
    parse_seterror("Unauthorized");
    return -1;
showhelp:
    if (html) {
        parse_seterror("Show Help HTML");
    } else {
        parse_seterror("Show Help TEXT");
    }
    return -1;
}
