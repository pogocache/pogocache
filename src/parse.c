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
// Unit parse.c provides the entrypoint for parsing all data 
// for incoming client connections.
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "parse.h"
#include "util.h"

__thread char parse_lasterr[1024] = "";

const char *parse_lasterror(void) {
    return parse_lasterr;
}

ssize_t parse_resp(const char *bytes, size_t len, struct args *args);
ssize_t parse_memcache(const char *data, size_t len, struct args *args,
    bool *noreply);
ssize_t parse_http(const char *data, size_t len, struct args *args,
    int *httpvers, bool *keepalive);
ssize_t parse_resp_telnet(const char *bytes, size_t len, struct args *args);
ssize_t parse_postgres(const char *data, size_t len, struct args *args,
    struct pg **pg);

static bool sniff_proto(const char *data, size_t len, int *proto) {
    if (len > 0 && data[0] == '*') {
        *proto = PROTO_RESP;
        return true;
    }
    if (len > 0 && data[0] == '\0') {
        *proto = PROTO_POSTGRES;
        return true;
    }
    // Parse the first line of text
    size_t n = 0;
    for (size_t i = 0; i < len; i++) {
        if (data[i] == '\n') {
            n = i+1;
            break;
        }
    }
    // Look for " HTTP/*.*\r\n" suffix
    if (n >= 11 && memcmp(data+n-11, " HTTP/", 5) == 0 && 
        data[n-4] == '.' && data[n-2] == '\r')
    {
        *proto = PROTO_HTTP;
        return true;
    }
    // Trim the prefix, Resp+Telnet and Memcache both allow for spaces between
    // arguments.
    while (*data == ' ') {
        data++;
        n--;
        len--;
    }
    // Treat all uppercase commands as Resp+Telnet
    if (n > 0 && data[0] >= 'A' && data[0] <= 'Z') {
        *proto = PROTO_RESP;
        return true;
    }
    // Look for Memcache commands
    if (n >= 1) {
        *proto = PROTO_MEMCACHE;
        return true;
    }
    // Protocol is unknown
    *proto = 0;
    return false;
}

// Returns the number of bytes read from data.
// returns -1 on error
// returns 0 when there isn't enough data to complete a command.
// On success, the args and proto will be set to the command arguments and
// protocol type, respectively.
//
// It's required to set proto to 0 for the first command, per client.
// Then continue to provide the last known proto. 
// This allows for the parser to learn and predict the protocol for ambiguous
// protocols; like Resp+Telnet, Memcache+Text, HTTP, etc.
//
// The noreply param is an output param that is only set when the proto is
// memcache. The <noreply> argument is stripped from the args array,
// but made available to the caller in case it needs to be known.
//
// The keepalive param is an output param that is only set when the proto is
// http. It's used to let the caller know to keep the connection alive for
// another request.
ssize_t parse_command(const void *data, size_t len, struct args *args, 
    int *proto, bool *noreply, int *httpvers, bool *keepalive, struct pg **pg)
{
    args_clear(args);
    parse_lasterr[0] = '\0';
    *httpvers = 0;
    *noreply = false;
    *keepalive = false;
    // Sniff for the protocol. This should only happen once per client, upon
    // their first request.
    if (*proto == 0) {
        if (!sniff_proto(data, len, proto)) {
            // Unknown protocol
            goto fail;
        }
        if (*proto == 0) {
            // Not enough data to determine yet
            return 0;
        }
    }
    if (*proto == PROTO_RESP) {
        const uint8_t *bytes = data;
        if (bytes[0] == '*') {
            return parse_resp(data, len, args);
        } else {
            return parse_resp_telnet(data, len, args);
        }
    } else if (*proto == PROTO_MEMCACHE) {
        return parse_memcache(data, len, args, noreply);
    } else if (*proto == PROTO_HTTP) {
        return parse_http(data, len, args, httpvers, keepalive);
    } else if (*proto == PROTO_POSTGRES) {
        return parse_postgres(data, len, args, pg);
    }
fail:
    parse_seterror("ERROR");
    return -1;
}

