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
// Unit conn.c are interface functions for a network connection.
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "net.h"
#include "args.h"
#include "cmds.h"
#include "xmalloc.h"
#include "parse.h"
#include "util.h"
#include "helppage.h"

#define MAXPACKETSZ 1048576 // Maximum read packet size

struct conn {
    struct net_conn *conn5; // originating connection
    struct buf packet;      // current incoming packet
    int proto;              // connection protocol (memcache, http, etc)
    bool auth;              // user is authorized
    bool noreply;           // only for memcache
    bool keepalive;         // only for http
    int httpvers;           // only for http
    struct args args;       // command args, if any
    struct pg *pg;          // postgres context, only if proto is postgres
};

bool conn_istls(struct conn *conn) {
    return net_conn_istls(conn->conn5);
}

int conn_proto(struct conn *conn) {
    return conn->proto;
}

bool conn_auth(struct conn *conn) {
    return conn->auth;
}

void conn_setauth(struct conn *conn, bool ok) {
    conn->auth = ok;
}

bool conn_isclosed(struct conn *conn) {
    return net_conn_isclosed(conn->conn5);
}

void conn_close(struct conn *conn) {
    net_conn_close(conn->conn5);
}

void evopened(struct net_conn *conn5, void *udata) {
    (void)udata;
    struct conn *conn = xmalloc(sizeof(struct conn));
    memset(conn, 0, sizeof(struct conn));
    conn->conn5 = conn5;
    net_conn_setudata(conn5, conn);
}

void evclosed(struct net_conn *conn5, void *udata) {
    (void)udata;
    struct conn *conn = net_conn_udata(conn5);
    buf_clear(&conn->packet);
    args_free(&conn->args);
    pg_free(conn->pg);
    xfree(conn);
}

// network data handler
// The evlen may be zero when returning from a bgwork routine, while having
// existing data in the connection packet.
void evdata(struct net_conn *conn5, const void *evdata, size_t evlen,
    void *udata)
{
    (void)udata;
    struct conn *conn = net_conn_udata(conn5);
    if (conn_isclosed(conn)) {
        goto close;
    }
#ifdef DATASETOK
    if (evlen == 14 && memcmp(evdata, "*1\r\n$4\r\nPING\r\n", 14) == 0) {
        conn_write_raw(conn, "+PONG\r\n", 7);
    } else if (evlen == 13 && memcmp(evdata, "*2\r\n$3\r\nGET\r\n", 13) == 0) {
        conn_write_raw(conn, "$1\r\nx\r\n", 7);
    } else {
        conn_write_raw(conn, "+OK\r\n", 5);
    }
    return;
#endif
    char *data;
    size_t len;
    bool copied;
    if (conn->packet.len == 0) {
        data = (char*)evdata;
        len = evlen;
        copied = false;
    } else {
        buf_append(&conn->packet, evdata, evlen);
        len = conn->packet.len;
        data = conn->packet.data;
        copied = true;
    }
    while (len > 0 && !conn_isclosed(conn)) {
        // Parse the command
        ssize_t n = parse_command(data, len, &conn->args, &conn->proto, 
            &conn->noreply, &conn->httpvers, &conn->keepalive, &conn->pg);
        if (n == 0) {
            // Not enough data provided yet.
            break;
        } else if (n == -1) {
            // Protocol error occurred.
            conn_write_error(conn, parse_lasterror());
            if (conn->proto == PROTO_MEMCACHE) {
                // Memcache doesn't close, but we'll need to know the last
                // character position to continue and revert back to it so
                // we can attempt to continue to the next command.
                n = parse_lastmc_n();
            } else {
                // Close on protocol error
                conn_close(conn);
                break;
            }
        } else if (conn->args.len == 0) {
            // There were no command arguments provided.
            if (conn->proto == PROTO_POSTGRES) {
                if (!pg_respond(conn, conn->pg)) {
                    // close connection
                    conn_close(conn);
                    break;
                }
            } else if (conn->proto == PROTO_MEMCACHE) {
                // Memcache simply returns a nondescript error.
                conn_write_error(conn, "ERROR");
            } else if (conn->proto == PROTO_HTTP) {
                // HTTP must always return arguments.
                assert(!"PROTO_HTTP");
            } else if (conn->proto == PROTO_RESP) {
                // RESP just continues until it gets args.
            }
        } else if (conn->proto == PROTO_POSTGRES && !conn->pg->ready) {
            // This should not have been reached. The client did not 
            // send a startup message
            conn_close(conn);
            break;
        } else if (conn->proto != PROTO_POSTGRES || 
            pg_precommand(conn, &conn->args, conn->pg))
        {
            evcommand(conn, &conn->args);
        }
        len -= n;
        data += n;
        if (net_conn_bgworking(conn->conn5)) {
            // BGWORK(0)
            break;
        }
        if (conn->proto == PROTO_HTTP) {
            conn_close(conn);
        }
    }
    if (conn_isclosed(conn)) {
        goto close;
    }
    if (len == 0) {
        if (copied) {
            if (conn->packet.cap > MAXPACKETSZ) {
                buf_clear(&conn->packet);
            }
            conn->packet.len = 0;
        }
    } else {
        if (copied) {
            memmove(conn->packet.data, data, len);
            conn->packet.len = len;
        } else {
            buf_append(&conn->packet, data, len);
        }
    }
    return;
close:
    conn_close(conn);
}

struct bgworkctx {
    struct conn *conn;
    void *udata;
    void(*work)(void *udata);
    void(*done)(struct conn *conn, void *udata);
};

static void work5(void *udata) {
    struct bgworkctx *ctx = udata;
    ctx->work(ctx->udata);
}

static void done5(struct net_conn *conn, void *udata) {
    (void)conn;
    struct bgworkctx *ctx = udata;
    ctx->done(ctx->conn, ctx->udata);
    xfree(ctx);
}

// conn_bgwork processes work in a background thread.
// When work is finished, the done function is called.
// It's not safe to use the conn type in the work function.
bool conn_bgwork(struct conn *conn, void(*work)(void *udata), 
    void(*done)(struct conn *conn, void *udata), void *udata)
{
    struct bgworkctx *ctx = xmalloc(sizeof(struct bgworkctx));
    ctx->conn = conn;
    ctx->udata = udata;
    ctx->work = work;
    ctx->done = done;
    if (!net_conn_bgwork(conn->conn5, work5, done5, ctx)) {
        xfree(ctx);
        return false;
    }
    return true;
}

static void writeln(struct conn *conn, char ch, const void *data, ssize_t len) {
    if (len < 0) {
        len = strlen(data);
    }
    net_conn_out_ensure(conn->conn5, 3+len);
    net_conn_out_write_byte_nocheck(conn->conn5, ch);
    size_t mark = net_conn_out_len(conn->conn5);
    net_conn_out_write_nocheck(conn->conn5, data, len);
    net_conn_out_write_byte_nocheck(conn->conn5, '\r');
    net_conn_out_write_byte_nocheck(conn->conn5, '\n');
    uint8_t *out = (uint8_t*)net_conn_out(conn->conn5);
    for (ssize_t i = mark; i < len; i++) {
        if (out[i] < ' ') {
            out[i] = ' ';
        }
    }
}

static void write_error(struct conn *conn, const char *err, bool server) {
    if (conn->proto == PROTO_MEMCACHE) {
        if (strstr(err, "ERR ") == err) {
            // convert to client or server error
            size_t err2sz = strlen(err)+32;
            char *err2 = xmalloc(err2sz);
            if (server) {
                snprintf(err2, err2sz, "SERVER_ERROR %s\r\n", err+4); 
            } else {
                snprintf(err2, err2sz, "CLIENT_ERROR %s\r\n", err+4); 
            }
            conn_write_raw(conn, err2, strlen(err2));
            xfree(err2);
        } else {
            if (server) {
                size_t err2sz = strlen(err)+32;
                char *err2 = xmalloc(err2sz);
                snprintf(err2, err2sz, "SERVER_ERROR %s\r\n", err);
                conn_write_raw(conn, err2, strlen(err2));
                xfree(err2);
            } else if (strstr(err, "CLIENT_ERROR ") == err || 
                strstr(err, "CLIENT_ERROR ") == err)
            {
                size_t err2sz = strlen(err)+32;
                char *err2 = xmalloc(err2sz);
                snprintf(err2, err2sz, "%s\r\n", err);
                conn_write_raw(conn, err2, strlen(err2));
                xfree(err2);
            } else {
                conn_write_raw(conn, "ERROR\r\n", 7);
            }
        }
    } else if (conn->proto == PROTO_POSTGRES) {
        if (strstr(err, "ERR ") == err) {
            err = err+4;
        }
        pg_write_error(conn, err);
        pg_write_ready(conn, 'I');
    } else if (conn->proto == PROTO_HTTP) {
        if (strstr(err, "ERR ") == err) {
            err += 4;
        }
        if (strcmp(err, "Show Help HTML") == 0) {
            conn_write_http(conn, 200, "OK", HELPPAGE_HTML, -1);
        } else if (strcmp(err, "Show Help TEXT") == 0) {
            conn_write_http(conn, 200, "OK", HELPPAGE_TEXT, -1);
        } else if (strcmp(err, "Method Not Allowed") == 0) {
            conn_write_http(conn, 405, "Method Not Allowed", 
                "Method Not Allowed\r\n", -1);
        } else if (strcmp(err, "Unauthorized") == 0) {
            conn_write_http(conn, 401, "Unauthorized", 
                "Unauthorized\r\n", -1);
        } else if (strcmp(err, "Bad Request") == 0) {
            conn_write_http(conn, 400, "Bad Request", 
                "Bad Request\r\n", -1);
        } else {
            size_t sz = strlen(err)+32;
            char *err2 = xmalloc(sz);
            snprintf(err2, sz, "ERR %s\r\n", err);
            conn_write_http(conn, 500, "Internal Server Error", 
                err2, -1);
            xfree(err2);
        }
    } else {
        writeln(conn, '-', err, -1);
    }
}

void conn_write_error(struct conn *conn, const char *err) {
    bool server = false;
    if (strcmp(err, ERR_OUT_OF_MEMORY) == 0) {
        server = true;
    }
    write_error(conn, err, server);
}

void conn_write_string(struct conn *conn, const char *cstr) {
    writeln(conn, '+', cstr, -1);
}

void conn_write_null(struct conn *conn) {
    net_conn_out_write(conn->conn5, "$-1\r\n", 5);
}

void resp_write_bulk(struct buf *buf, const void *data, size_t len) {
    uint8_t str[32];
    size_t n = u64toa(len, str);
    buf_append_byte(buf, '$');
    buf_append(buf, str, n);
    buf_append_byte(buf, '\r');
    buf_append_byte(buf, '\n');
    buf_append(buf, data, len);
    buf_append_byte(buf, '\r');
    buf_append_byte(buf, '\n');
}

void conn_write_bulk(struct conn *conn, const void *data, size_t len) {
    net_conn_out_ensure(conn->conn5, 32+len);
    size_t olen = net_conn_out_len(conn->conn5);
    uint8_t *base = (uint8_t*)net_conn_out(conn->conn5)+olen;
    uint8_t *p = base;
    *(p++) = '$';
    p += u64toa(len, p);
    *(p++) = '\r';
    *(p++) = '\n';
    memcpy(p, data, len);
    p += len;
    *(p++) = '\r';
    *(p++) = '\n';
    net_conn_out_setlen(conn->conn5, olen + (p-base));
}

void conn_write_raw(struct conn *conn, const void *data, size_t len) {
    net_conn_out_write(conn->conn5, data, len);
}

void conn_write_http(struct conn *conn, int code, const char *status,
    const void *body, ssize_t bodylen)
{
    if (bodylen == -1) {
        if (!body) {
            body = status;
        }
        bodylen = strlen(body);
    }
    char resp[512];
    size_t n = snprintf(resp, sizeof(resp), 
        "HTTP/1.1 %d %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: Close\r\n"
        "\r\n",
        code, status, bodylen);
    conn_write_raw(conn, resp, n);
    if (bodylen > 0) {
        conn_write_raw(conn, body, bodylen);
    }
}

void conn_write_array(struct conn *conn, size_t count) {
    uint8_t str[24];
    size_t n = u64toa(count, str);
    writeln(conn, '*', str, n);
}

void conn_write_uint(struct conn *conn, uint64_t value) {
    uint8_t buf[24];
    size_t n = u64toa(value, buf);
    if (conn->proto == PROTO_MEMCACHE) {
        conn_write_raw(conn, buf, n);
    } else {
        writeln(conn, '+', buf, n); // the '+' is needed for unsigned int
    }
}

void conn_write_int(struct conn *conn, int64_t value) {
    uint8_t buf[24];
    size_t n = i64toa(value, buf);
    if (conn->proto == PROTO_MEMCACHE) {
        conn_write_raw(conn, buf, n);
    } else {
        writeln(conn, ':', buf, n);
    }
}

void conn_write_raw_cstr(struct conn *conn, const char *cstr) {
    conn_write_raw(conn, cstr, strlen(cstr));
}

void conn_write_bulk_cstr(struct conn *conn, const char *cstr) {
    conn_write_bulk(conn, cstr, strlen(cstr));
}

void stat_cmd_get_incr(struct conn *conn) {
    net_stat_cmd_get_incr(conn->conn5);
}

void stat_cmd_set_incr(struct conn *conn) {
    net_stat_cmd_set_incr(conn->conn5);
}

void stat_get_hits_incr(struct conn *conn) {
    net_stat_get_hits_incr(conn->conn5);
}

void stat_get_misses_incr(struct conn *conn) {
    net_stat_get_misses_incr(conn->conn5);
}

bool pg_execute(struct conn *conn) {
    return conn->pg->execute;
}

struct pg *conn_pg(struct conn *conn) {
    return conn->pg;
}
