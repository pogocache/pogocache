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
// Unit monitor.c provides functions for the MONITOR command.
#include <stdatomic.h>
#include <pthread.h>
#include "monitor.h"
#include "util.h"
#include "sys.h"
#include "xmalloc.h"

static atomic_int monitoring = 0;
static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static struct conn **conns = 0;
static int conns_len = 0;
static int conns_cap = 0;

void monitor_start(struct conn *conn) {
    pthread_mutex_lock(&mu);
    if (conns_len == conns_cap) {
        conns_cap = conns_cap == 0 ? 1 : conns_cap * 2;
        conns = xrealloc(conns, conns_cap*sizeof(struct conn*));
    }
    conns[conns_len++] = conn;
    atomic_fetch_add(&monitoring, 1);
    pthread_mutex_unlock(&mu);
}

void monitor_stop(struct conn *conn) {
    pthread_mutex_lock(&mu);
    for (int i = 0; i < conns_len; i++) {
        if (conns[i] == conn) {
            conns[i] = conns[conns_len-1];
            conns_len--;
            break;
        }
    }
    atomic_fetch_sub(&monitoring, 1);
    pthread_mutex_unlock(&mu);
}

void monitor_cmd(int64_t now, int db, const char *addr, struct args *args) {
    if (atomic_load_explicit(&monitoring, __ATOMIC_RELAXED) == 0) {
        // Nothing to monitor
        return;
    }
    if (!args || args->len == 0 || args_eq(args, 0, "auth") || 
        args_eq(args, 0, "quit") || args_eq(args, 0, "monitor"))
    {
        return;
    }
    struct buf buf = { 0 };
    buf_ensure(&buf, 256);
    buf.len = 0;
    size_t n = snprintf(buf.data, 256, "+%.6f [%d %s]", 
        (double)now / (double)SECOND, db, addr);
    buf.len = n;
    for (size_t i = 0; i < args->len; i++) {
        buf_append(&buf, " \"", 2);
        for (size_t j = 0; j < args->bufs[i].len; j++) {
            unsigned char ch = args->bufs[i].data[j];
            switch (ch) {
            case '\n':
                buf_append(&buf, "\\n", 2);
                break;
            case '\r':
                buf_append(&buf, "\\r", 2);
                break;
            case '\t':
                buf_append(&buf, "\\t", 2);
                break;
            case '\"':
                buf_append(&buf, "\\\"", 2);
                break;
            case '\\':
                buf_append(&buf, "\\\\", 2);
                break;
            default:
                if (ch < 32 || ch >= 127) {
                    char hex[8];
                    snprintf(hex, 8, "\\x%02X", ch);
                    buf_append(&buf, hex, 8);
                } else {
                    buf_append_byte(&buf, ch);
                }
            }
        }
        buf_append(&buf, "\"", 1);
    }
    buf_append(&buf, "\r\n", 2);

    pthread_mutex_lock(&mu);
    for (int i = 0; i < conns_len; i++) {
        conn_write(conns[i], buf.data, buf.len);
    }
    
    pthread_mutex_unlock(&mu);

    buf_clear(&buf);
}
