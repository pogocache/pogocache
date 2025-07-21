// https://github.com/tidwall/pogocache
//
// Copyright 2025 Polypoint Labs, LLC. All rights reserved.
// This file is part of the Pogocache project.
// Use of this source code is governed by the AGPL that can be found in
// the LICENSE file.
//
// For alternative licensing options or general questions, please contact
// us at licensing@polypointlabs.com.
#ifndef NET_H
#define NET_H

#include <stdbool.h>
#include <stdlib.h>
#include "buf.h"

struct net_conn;

void net_conn_setudata(struct net_conn *conn, void *udata);
void *net_conn_udata(struct net_conn *conn);

bool net_conn_isclosed(struct net_conn *conn);
void net_conn_close(struct net_conn *conn);

// Get the raw output.
char *net_conn_out(struct net_conn *conn);
size_t net_conn_out_len(struct net_conn *conn);
size_t net_conn_out_cap(struct net_conn *conn);

// Write to the output buffer
void net_conn_out_write_byte(struct net_conn *conn, char byte);
void net_conn_out_write(struct net_conn *conn, const void *data,
    size_t nbytes);

// write to output buffer, but do not check bounds.
// Probably a good idea to call the net_conn_out_ensure first.
void net_conn_out_ensure(struct net_conn *conn, size_t amount);
void net_conn_out_setlen(struct net_conn *conn, size_t len);
void net_conn_out_write_byte_nocheck(struct net_conn *conn, char byte);
void net_conn_out_write_nocheck(struct net_conn *conn, const void *data,
    size_t nbytes);

struct net_opts {
    const char *host;
    const char *port;
    const char *tlsport; 
    const char *unixsock;
    bool reuseport;
    bool tcpnodelay;
    bool keepalive;
    bool quickack;
    int backlog;
    int queuesize;
    int nthreads;
    int maxconns;
    bool nowarmup;
    bool nouring;
    void *udata;
    void(*listening)(void *udata);
    void(*ready)(void *udata);
    void(*data)(struct net_conn *conn, const void *data, size_t nbytes, 
        void *udata);
    void(*opened)(struct net_conn *conn, void *udata);
    void(*closed)(struct net_conn *conn, void *udata);
};

void net_main(struct net_opts *opts);

size_t net_nconns(void);
size_t net_tconns(void);
size_t net_rconns(void);

bool net_conn_bgwork(struct net_conn *conn, void (*work)(void *udata), 
    void (*done)(struct net_conn *conn, void *udata), void *udata);
bool net_conn_bgworking(struct net_conn *conn);
bool net_conn_istls(struct net_conn *conn);

// Some stats are collected in the connection and summed in the event loop.
void net_stat_cmd_get_incr(struct net_conn *conn);
void net_stat_cmd_set_incr(struct net_conn *conn);
void net_stat_get_hits_incr(struct net_conn *conn);
void net_stat_get_misses_incr(struct net_conn *conn);

uint64_t stat_cmd_get(void);
uint64_t stat_cmd_set(void);
uint64_t stat_get_hits(void);
uint64_t stat_get_misses(void);

#endif
