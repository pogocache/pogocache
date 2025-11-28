// https://github.com/tidwall/pogocache
//
// Copyright 2025 Polypoint Labs, LLC. All rights reserved.
// This file is part of the Pogocache project.
// Use of this source code is governed by the MIT that can be found in
// the LICENSE file.
//
// For alternative licensing options or general questions, please contact
// us at licensing@polypointlabs.com.
#ifndef CONN_H
#define CONN_H

#include <stdio.h>
#include "net.h"

#define PROTO_MEMCACHE 1
#define PROTO_POSTGRES 2
#define PROTO_HTTP     3
#define PROTO_RESP     4

#define ERR_WRONG_NUM_ARGS      "ERR wrong number of arguments"
#define ERR_SYNTAX_ERROR        "ERR syntax error"
#define ERR_INDEX_OUT_OF_RANGE	"ERR index is out of range"
#define ERR_INVALID_INTEGER     "ERR value is not an integer or out of range"
#define ERR_OUT_OF_MEMORY       "ERR out of memory"
#define CLIENT_ERROR_BAD_FORMAT "CLIENT_ERROR bad command line format"
#define CLIENT_ERROR_BAD_CHUNK  "CLIENT_ERROR bad data chunk"

struct conn;

void conn_close(struct conn *conn);
bool conn_isclosed(struct conn *conn);
bool conn_istls(struct conn *conn);

// only use these from bgwork threads
ssize_t conn_read(struct conn *conn, char *bytes, size_t nbytes);
ssize_t conn_write(struct conn *conn, const char *bytes, size_t nbytes);
int conn_fd(struct conn *conn);
int conn_setnonblock(struct conn *conn, bool set);

const char *conn_addr(struct conn *conn);

void conn_write_error(struct conn *conn, const char *err);
void conn_write_raw(struct conn *conn, const void *data, size_t len);
void conn_write_string(struct conn *conn, const char *cstr);
void conn_write_null(struct conn *conn);
void conn_write_bulk(struct conn *conn, const void *data, size_t len);
void conn_write_array(struct conn *conn, size_t count);
void conn_write_raw_cstr(struct conn *conn, const char *cstr);

void conn_write_http(struct conn *conn, int code, const char *status,
    const void *body, ssize_t bodylen);
void conn_write_uint(struct conn *conn, uint64_t value);
void conn_write_int(struct conn *conn, int64_t value);
void conn_write_bulk_cstr(struct conn *conn, const char *cstr);

void resp_write_bulk(struct buf *buf, const void *data, size_t len);

int conn_proto(struct conn *conn);
bool conn_auth(struct conn *conn);
void conn_setauth(struct conn *conn, bool authorized);

bool pg_execute(struct conn *conn);

void pg_write_row_desc(struct conn *conn, const char **fields, int nfields);
void pg_write_row_data(struct conn *conn, const char **cols, 
    const size_t *collens, int ncols);
void pg_write_error(struct conn *conn, const char *msg);
void pg_write_complete(struct conn *conn, const char *tag);
void pg_write_completef(struct conn *conn, const char *tag_format, ...);
void pg_write_ready(struct conn *conn, unsigned char code);
void pg_write_simple_row_data_ready(struct conn *conn, const char *desc,
    const void *row, size_t len, const char *tag);
void pg_write_simple_row_i64_ready(struct conn *conn, const char *desc,
    int64_t row, const char *tag);
void pg_write_simple_row_str_ready(struct conn *conn, const char *desc,
    const char *row, const char *tag);
void pg_write_simple_row_i64_readyf(struct conn *conn, const char *desc,
    int64_t row, const char *tag_format, ...);
void pg_write_simple_row_str_readyf(struct conn *conn, const char *desc,
    const char *row, const char *tag_format, ...);

bool conn_bgwork(struct conn *conn, void(*work)(void *udata), 
    void(*done)(struct conn *conn, void *udata), void *udata);

void stat_cmd_get_incr(struct conn *conn);
void stat_cmd_set_incr(struct conn *conn);
void stat_get_hits_incr(struct conn *conn);
void stat_get_misses_incr(struct conn *conn);

struct pg *conn_pg(struct conn *conn);

// net event handlers

void evopened(struct net_conn *conn, void *udata);
void evclosed(struct net_conn *conn, void *udata);
void evdata(struct net_conn *conn, const void *data, size_t len, void *udata);

#endif
