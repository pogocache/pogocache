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
// Unit resp.c provides the parser for the RESP wire protocol.
#include <assert.h>
#include <limits.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include "util.h"
#include "stats.h"
#include "parse.h"

// returns the number of bytes read from data.
// returns -1 on error
// returns 0 when there isn't enough data to complete a command.
ssize_t parse_resp_telnet(const char *bytes, size_t len, struct args *args) {
    char *err = NULL;
    struct buf arg = { 0 };
    bool inarg = false;
    char quote = '\0';
    for (size_t i = 0; i < len; i++) {
        char ch = bytes[i];
        if (inarg) {
            if (quote) {
                if (ch == '\n') {
                    goto fail_quotes;
                }
                if (ch == quote) { 
                    args_append(args, arg.data, arg.len, false);
                    if (args->len > MAXARGS) {
                        goto fail_nargs;
                    }
                    arg.len = 0;
                    i++;
                    if (i == len) {
                        break;
                    }
                    ch = bytes[i];
                    inarg = false;
                    if (ch == '\n') {
                        i--;
                        continue;
                    }
                    if (!isspace(ch)) {
                        goto fail_quotes;
                    }
                    continue;
                } else if (ch == '\\') {
                    i++;
                    if (i == len) {
                        break;
                    }
                    ch = bytes[i];
                    switch (ch) {
                    case 'n': ch = '\n'; break;
                    case 'r': ch = '\r'; break;
                    case 't': ch = '\t'; break;
                    }
                }
                buf_append_byte(&arg, ch);
                if (arg.len > MAXARGSZ) {
                    stat_store_too_large_incr(0);
                    goto fail_argsz;
                }
            } else {
                if (ch == '"' || ch == '\'') {
                    quote = ch;
                } else if (isspace(ch)) {
                    args_append(args, arg.data, arg.len, false);
                    if (args->len > MAXARGS) {
                        goto fail_nargs;
                    }
                    arg.len = 0;
                    if (ch == '\n') {
                        break;
                    }
                    inarg = false;
                } else {
                    buf_append_byte(&arg, ch);
                    if (arg.len > MAXARGSZ) {
                        stat_store_too_large_incr(0);
                        goto fail_argsz;
                    }
                }
            }
        } else {
            if (ch == '\n') {
                buf_clear(&arg);
                return i+1;
            }
            if (isspace(ch)) {
                continue;
            }
            inarg = true;
            if (ch == '"' || ch == '\'') {
                quote = ch;
            } else {
                quote = 0;
                buf_append_byte(&arg, ch);
                if (arg.len > MAXARGSZ) {
                    stat_store_too_large_incr(0);
                    goto fail_argsz;
                }
            }
        }
    }
    buf_clear(&arg);
    return 0;
fail_quotes:
    if (!err) err = "ERR Protocol error: unbalanced quotes in request";
fail_nargs:
    if (!err) err = "ERR Protocol error: invalid multibulk length";
fail_argsz:
    if (!err) err = "ERR Protocol error: invalid bulk length";
/* fail: */
    if (err) {
        snprintf(parse_lasterr, sizeof(parse_lasterr), "%s", err);
    }
    buf_clear(&arg);
    return -1;
}

static int64_t read_num(const char *data, size_t len, int64_t min, int64_t max,
    bool *ok)
{
    errno = 0;
    char *end;
    int64_t x = strtoll(data, &end, 10);
    *ok = errno == 0 && (size_t)(end-data) == len && x >= min && x <= max;
    return x;
}

#define read_resp_num(var, min, max, errmsg) { \
    char *p = memchr(bytes, '\r', end-bytes); \
    if (!p) { \
        if (end-bytes > 32) { \
            parse_seterror("ERR Protocol error: " errmsg); \
            return -1; \
        } \
        return 0; \
    } \
    if (p+1 == end) { \
        return 0; \
    } \
    if (*(p+1) != '\n') { \
        return -1; \
    } \
    bool ok; \
    var = read_num(bytes, p-bytes, min, max, &ok); \
    if (!ok) { \
        parse_seterror("ERR Protocol error: " errmsg); \
        return -1; \
    } \
    bytes = p+2; \
}

// returns the number of bytes read from data.
// returns -1 on error
// returns 0 when there isn't enough data to complete a command.
ssize_t parse_resp(const char *bytes, size_t len, struct args *args) {
    const char *start = bytes;
    const char *end = bytes+len;
    if (bytes == end) {
        return 0;
    }
    if (*(bytes++) != '*') {
        return -1;
    }
    if (bytes == end) {
        return 0;
    }
    int64_t nargs;
    read_resp_num(nargs, LONG_MIN, MAXARGS, "invalid multibulk length");
    for (int j = 0; j < nargs; j++) {
        if (bytes == end) {
            return 0;
        }
        if (*(bytes++) != '$') {
            snprintf(parse_lasterr, sizeof(parse_lasterr), 
                "ERR Protocol error: expected '$', got '%c'", *(bytes-1));
            return -1;
        }
        if (bytes == end) {
            return 0;
        }
        int64_t nbytes;
        read_resp_num(nbytes, 0, MAXARGSZ, "invalid bulk length");
        if (nbytes+2 > end-bytes) {
            return 0;
        }
        args_append(args, bytes, nbytes, true);
        bytes += nbytes+2;
    }
    return bytes-start;
}

