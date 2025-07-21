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
// Unit net.c provides most network functionality, including listening on ports,
// thread creation, event queue handling, and reading & writing sockets.
#define _GNU_SOURCE
#include <assert.h>
#include <signal.h>
#include <stdatomic.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include <time.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>
#include <inttypes.h>
#include <ctype.h>
#include <sys/un.h>

#ifdef __linux__
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#else
#include <sys/event.h>
#endif

#include "uring.h"
#include "stats.h"
#include "net.h"
#include "util.h"
#include "tls.h"
#include "xmalloc.h"

#define PACKETSIZE 16384
#define MINURINGEVENTS 2 // there must be at least 2 events for uring use

extern const int verb;

static int setnonblock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        return -1;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int settcpnodelay(int fd, bool nodelay) {
    int val = nodelay;
    return setsockopt(fd, SOL_SOCKET, TCP_NODELAY, &val, sizeof(val)) == 0;
}

static int setquickack(int fd, bool quickack) {
#if defined(__linux__)
    int val = quickack;
    return setsockopt(fd, SOL_SOCKET, TCP_QUICKACK, &val, sizeof(val)) == 0;
#else
    (void)fd, (void)quickack;
    return 0;
#endif
}

static int setkeepalive(int fd, bool keepalive) {
    int val = keepalive;
    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &val, sizeof(val))) {
        return -1;
    }
#if defined(__linux__)
    if (!keepalive) {
        return 0;
    }
    // tcp_keepalive_time
    if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &(int){300}, sizeof(int))) 
    {
        return -1;
    }
    // tcp_keepalive_intvl
    if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &(int){30}, sizeof(int)))
    {
        return -1;
    }
    // tcp_keepalive_probes
    if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &(int){3}, sizeof(int))) {
        return -1;
    }
#endif
    return 0;
}

#ifdef __linux__
typedef struct epoll_event event_t;
#else
typedef struct kevent event_t;
#endif

static int event_fd(event_t *ev) {
#ifdef __linux__
    return ev->data.fd;
#else
    return ev->ident;
#endif
}

static int getevents(int fd, event_t evs[], int nevs, bool wait_forever, 
    int64_t timeout)
{
    if (wait_forever) {
#ifdef __linux__
        return epoll_wait(fd, evs, nevs, -1);
#else
        return kevent(fd, NULL, 0, evs, nevs, 0);
#endif
    } else {
        timeout = timeout < 0 ? 0 : 
        timeout > 900000000 ? 900000000 : // 900ms
        timeout;
#ifdef __linux__
        timeout = timeout / 1000000;
        return epoll_wait(fd, evs, nevs, timeout);
#else
        struct timespec timespec = { .tv_nsec = timeout };
        return kevent(fd, NULL, 0, evs, nevs, &timespec);
#endif
    }
}

static int addread(int qfd, int fd) {
#ifdef __linux__
    struct epoll_event ev = { 0 };
    ev.events = EPOLLIN | EPOLLEXCLUSIVE;
    ev.data.fd = fd;
    return epoll_ctl(qfd, EPOLL_CTL_ADD, fd, &ev);
#else
    struct kevent ev={.filter=EVFILT_READ,.flags=EV_ADD,.ident=(fd)};
    return kevent(qfd, &ev, 1, NULL, 0, NULL);
#endif
}

static int delread(int qfd, int fd) {
#ifdef __linux__
    struct epoll_event ev = { 0 };
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    return epoll_ctl(qfd, EPOLL_CTL_DEL, fd, &ev);
#else
    struct kevent ev={.filter=EVFILT_READ,.flags=EV_DELETE,.ident=(fd)};
    return kevent(qfd, &ev, 1, NULL, 0, NULL);
#endif
}

static int addwrite(int qfd, int fd) {
#ifdef __linux__
    struct epoll_event ev = { 0 };
    ev.events = EPOLLOUT;
    ev.data.fd = fd;
    return epoll_ctl(qfd, EPOLL_CTL_ADD, fd, &ev);
#else
    struct kevent ev={.filter=EVFILT_WRITE,.flags=EV_ADD,.ident=(fd)};
    return kevent(qfd, &ev, 1, NULL, 0, NULL);
#endif
}

static int delwrite(int qfd, int fd) {
#ifdef __linux__
    struct epoll_event ev = { 0 };
    ev.events = EPOLLOUT;
    ev.data.fd = fd;
    return epoll_ctl(qfd, EPOLL_CTL_DEL, fd, &ev);
#else
    struct kevent ev={.filter=EVFILT_WRITE,.flags=EV_DELETE,.ident=(fd)};
    return kevent(qfd, &ev, 1, NULL, 0, NULL);
#endif
}

static int evqueue(void) {
#ifdef __linux__
    return epoll_create1(0);
#else
    return kqueue();
#endif
}

struct bgworkctx { 
    void (*work)(void *udata);
    void (*done)(struct net_conn *conn, void *udata);
    struct net_conn *conn;
    void *udata;
    bool writer;
};

// static void bgdone(struct bgworkctx *bgctx);

struct net_conn {
    int fd;
    struct net_conn *next; // for hashmap bucket
    bool closed;
    struct tls *tls;
    void *udata;
    char *out;
    size_t outlen;
    size_t outcap;
    struct bgworkctx *bgctx;
    struct qthreadctx *ctx;
    unsigned stat_cmd_get;
    unsigned stat_cmd_set;
    unsigned stat_get_hits;
    unsigned stat_get_misses;
};

static struct net_conn *conn_new(int fd, struct qthreadctx *ctx) {
    struct net_conn *conn = xmalloc(sizeof(struct net_conn));
    memset(conn, 0, sizeof(struct net_conn));
    conn->fd = fd;
    conn->ctx = ctx;
    return conn;
}

static void conn_free(struct net_conn *conn) {
    if (conn) {
        if (conn->out) {
            xfree(conn->out);
        }
        xfree(conn);
    }
}

void net_conn_out_ensure(struct net_conn *conn, size_t amount) {
    if (conn->outcap-conn->outlen >= amount) {
        return;
    }
    size_t cap = conn->outcap == 0 ? 16 : conn->outcap * 2;
    while (cap-conn->outlen < amount) {
        cap *= 2;
    }
    char *out = xmalloc(cap);
    memcpy(out, conn->out, conn->outlen);
    xfree(conn->out);
    conn->out = out;
    conn->outcap = cap;
}

void net_conn_out_write_byte_nocheck(struct net_conn *conn, char byte) {
    conn->out[conn->outlen++] = byte;
}

void net_conn_out_write_byte(struct net_conn *conn, char byte) {
    if (conn->outcap == conn->outlen) {
        net_conn_out_ensure(conn, 1);
    }
    net_conn_out_write_byte_nocheck(conn, byte);
}

void net_conn_out_write_nocheck(struct net_conn *conn, const void *data,
    size_t nbytes)
{
    memcpy(conn->out+conn->outlen, data, nbytes);
    conn->outlen += nbytes;
}

void net_conn_out_write(struct net_conn *conn, const void *data,
    size_t nbytes)
{
    if (conn->outcap-conn->outlen < nbytes) {
        net_conn_out_ensure(conn, nbytes);
    }
    net_conn_out_write_nocheck(conn, data, nbytes);
}

char *net_conn_out(struct net_conn *conn) {
    return conn->out;
}

size_t net_conn_out_len(struct net_conn *conn) {
    return conn->outlen;
}

size_t net_conn_out_cap(struct net_conn *conn) {
    return conn->outcap;
}

void net_conn_out_setlen(struct net_conn *conn, size_t len) {
    assert(len < conn->outcap);
    conn->outlen = len;
}


bool net_conn_isclosed(struct net_conn *conn) {
    return conn->closed;
}

void net_conn_close(struct net_conn *conn) {
    conn->closed = true;
}

void net_conn_setudata(struct net_conn *conn, void *udata) {
    conn->udata = udata;
}

void *net_conn_udata(struct net_conn *conn) {
    return conn->udata;
}

static uint64_t hashfd(int fd) {
    return mix13((uint64_t)fd);
}

// map of connections
struct cmap {
    struct net_conn **buckets;
    size_t nbuckets;
    size_t len;
};

static void cmap_insert(struct cmap *cmap, struct net_conn *conn);

static void cmap_grow(struct cmap *cmap) {
    struct cmap cmap2 = { 0 };
    cmap2.nbuckets = cmap->nbuckets*2;
    size_t size = cmap2.nbuckets * sizeof(struct net_conn*);
    cmap2.buckets = xmalloc(size);
    memset(cmap2.buckets, 0, cmap2.nbuckets*sizeof(struct net_conn*));
    for (size_t i = 0; i < cmap->nbuckets; i++) {
        struct net_conn *conn = cmap->buckets[i];
        while (conn) {
            struct net_conn *next = conn->next;
            conn->next = 0;
            cmap_insert(&cmap2, conn);
            conn = next;
        }
    }
    xfree(cmap->buckets);
    memcpy(cmap, &cmap2, sizeof(struct cmap));
}

// Insert a connection into a map. 
// The connection MUST NOT exist in the map.
static void cmap_insert(struct cmap *cmap, struct net_conn *conn) {
    uint32_t hash = hashfd(conn->fd);
    if (cmap->len >= cmap->nbuckets-(cmap->nbuckets>>2)) { // 75% load factor
    // if (cmap->len >= cmap->nbuckets) { // 100% load factor
        cmap_grow(cmap);
    }
    size_t i = hash % cmap->nbuckets;
    conn->next = cmap->buckets[i];
    cmap->buckets[i] = conn;
    cmap->len++;
}

// Return the connection or NULL if not exists.
static struct net_conn *cmap_get(struct cmap *cmap, int fd) {
    uint32_t hash = hashfd(fd);
    size_t i = hash % cmap->nbuckets;
    struct net_conn *conn = cmap->buckets[i];
    while (conn && conn->fd != fd) {
        conn = conn->next;
    }
    return conn;
}

// Delete connection from map. 
// The connection MUST exist in the map.
static void cmap_delete(struct cmap *cmap, struct net_conn *conn) {
    uint32_t hash = hashfd(conn->fd);
    size_t i = hash % cmap->nbuckets;
    struct net_conn *prev = 0;
    struct net_conn *iter = cmap->buckets[i];
    while (iter != conn) {
        prev = iter;
        iter = iter->next;
    }
    if (prev) {
        prev->next = iter->next;
    } else {
        cmap->buckets[i] = iter->next;
    }
}

static atomic_size_t nconns = 0;
static atomic_size_t tconns = 0;
static atomic_size_t rconns = 0;

static pthread_mutex_t tls_ready_fds_lock = PTHREAD_MUTEX_INITIALIZER;
static int tls_ready_fds_cap = 0;
static int tls_ready_fds_len = 0;
static int *tls_ready_fds = 0;

static void save_tls_fd(int fd) {
    pthread_mutex_lock(&tls_ready_fds_lock);
    if (tls_ready_fds_len == tls_ready_fds_cap) {
        tls_ready_fds_cap *= 2;
        if (tls_ready_fds_cap == 0) {
            tls_ready_fds_cap = 8;
        }
        tls_ready_fds = xrealloc(tls_ready_fds, tls_ready_fds_cap*sizeof(int));
    }
    tls_ready_fds[tls_ready_fds_len++] = fd;
    pthread_mutex_unlock(&tls_ready_fds_lock);
}

static bool del_tls_fd(int fd) {
    bool found = false;
    pthread_mutex_lock(&tls_ready_fds_lock);
    for (int i = 0; i < tls_ready_fds_len; i++) {
        if (tls_ready_fds[i] == fd) {
            tls_ready_fds[i] = tls_ready_fds[tls_ready_fds_len-1];
            tls_ready_fds_len--;
            found = true;
            break;
        }
    }
    pthread_mutex_unlock(&tls_ready_fds_lock);
    return found;
}

struct qthreadctx {
    pthread_t th;
    int qfd;
    int index;
    int maxconns;
    int *sfd;   // three entries
    bool tcpnodelay;
    bool keepalive;
    bool quickack;
    int queuesize;
    const char *unixsock;
    void *udata;
    bool uring;
#ifndef NOURING
    struct io_uring ring;
#endif
    void(*data)(struct net_conn*,const void*,size_t,void*);
    void(*opened)(struct net_conn*,void*);
    void(*closed)(struct net_conn*,void*);
    int nevents;
    event_t *events;
    atomic_int nconns;
    int ntlsconns;
    char *inpkts;
    struct net_conn **qreads;
    struct net_conn **qins;
    struct net_conn **qattachs;
    struct net_conn **qouts;
    struct net_conn **qcloses;
    char **qinpkts;
    int *qinpktlens;    
    int nqreads;
    int nqins;
    int nqcloses;
    int nqattachs;
    int nqouts;
    int nthreads;
    
    uint64_t stat_cmd_get;
    uint64_t stat_cmd_set;
    uint64_t stat_get_hits;
    uint64_t stat_get_misses;

    struct qthreadctx *ctxs;
    struct cmap cmap;
};

static atomic_uint_fast64_t g_stat_cmd_get = 0;
static atomic_uint_fast64_t g_stat_cmd_set = 0;
static atomic_uint_fast64_t g_stat_get_hits = 0;
static atomic_uint_fast64_t g_stat_get_misses = 0;

inline
static void sumstats(struct net_conn *conn, struct qthreadctx *ctx) {
    ctx->stat_cmd_get += conn->stat_cmd_get;
    conn->stat_cmd_get = 0;
    ctx->stat_cmd_set += conn->stat_cmd_set;
    conn->stat_cmd_set = 0;
    ctx->stat_get_hits += conn->stat_get_hits;
    conn->stat_get_hits = 0;
    ctx->stat_get_misses += conn->stat_get_misses;
    conn->stat_get_misses = 0;
}

inline
static void sumstats_global(struct qthreadctx *ctx) {
    atomic_fetch_add_explicit(&g_stat_cmd_get, ctx->stat_cmd_get, 
        __ATOMIC_RELAXED);
    ctx->stat_cmd_get = 0;
    atomic_fetch_add_explicit(&g_stat_cmd_set, ctx->stat_cmd_set, 
        __ATOMIC_RELAXED);
    ctx->stat_cmd_set = 0;
    atomic_fetch_add_explicit(&g_stat_get_hits, ctx->stat_get_hits, 
        __ATOMIC_RELAXED);
    ctx->stat_get_hits = 0;
    atomic_fetch_add_explicit(&g_stat_get_misses, ctx->stat_get_misses, 
        __ATOMIC_RELAXED);
    ctx->stat_get_misses = 0;
}

uint64_t stat_cmd_get(void) {
    uint64_t x = atomic_load_explicit(&g_stat_cmd_get, __ATOMIC_RELAXED);
    return x;
}

uint64_t stat_cmd_set(void) {
    return atomic_load_explicit(&g_stat_cmd_set, __ATOMIC_RELAXED);
}

uint64_t stat_get_hits(void) {
    return atomic_load_explicit(&g_stat_get_hits, __ATOMIC_RELAXED);
}

uint64_t stat_get_misses(void) {
    return atomic_load_explicit(&g_stat_get_misses, __ATOMIC_RELAXED);
}

inline
static void qreset(struct qthreadctx *ctx) {
    ctx->nqreads = 0;
    ctx->nqins = 0;
    ctx->nqcloses = 0;
    ctx->nqouts = 0;
    ctx->nqattachs = 0;
}

inline
static void qaccept(struct qthreadctx *ctx) {
    for (int i = 0; i < ctx->nevents; i++) {
        int fd = event_fd(&ctx->events[i]);
        struct net_conn *conn = cmap_get(&ctx->cmap, fd);
        if (!conn) {
            if ((fd == ctx->sfd[0] || fd == ctx->sfd[1] || fd == ctx->sfd[2])) {
                int sfd = fd;
                fd = accept(fd, 0, 0);
                if (fd == -1) {
                    continue;
                }
                if (setnonblock(fd) == -1) {
                    close(fd);
                    continue;
                }
                if (sfd == ctx->sfd[0] || sfd == ctx->sfd[2]) {
                    if (setkeepalive(fd, ctx->keepalive) == -1) {
                        close(fd);
                        continue;
                    }
                    if (settcpnodelay(fd, ctx->tcpnodelay) == -1) {
                        close(fd);
                        continue;
                    }
                    if (setquickack(fd, ctx->quickack) == -1) {
                        close(fd);
                        continue;
                    }
                    if (sfd == ctx->sfd[2]) {
                        save_tls_fd(fd);
                    }
                }
                static atomic_uint_fast64_t next_ctx_index = 0;
                int idx = atomic_fetch_add(&next_ctx_index, 1) % ctx->nthreads;
                if (addread(ctx->ctxs[idx].qfd, fd) == -1) {
                    if (sfd == ctx->sfd[2]) {
                        del_tls_fd(fd);
                    }
                    close(fd);
                    continue;
                }
                continue;
            }
            size_t xnconns = atomic_fetch_add(&nconns, 1);
            if (xnconns >= (size_t)ctx->maxconns) {
                // rejected
                atomic_fetch_add(&rconns, 1);
                atomic_fetch_sub(&nconns, 1);
                close(fd);
                continue;
            }
            bool istls = del_tls_fd(fd);
            conn = conn_new(fd, ctx);
            if (istls) {
                if (!tls_accept(conn->fd, &conn->tls)) {
                    atomic_fetch_sub(&nconns, 1);
                    close(fd);
                    conn_free(conn);
                    continue;
                }
                ctx->ntlsconns++;
            }
            atomic_fetch_add_explicit(&ctx->nconns, 1, __ATOMIC_RELEASE);
            atomic_fetch_add_explicit(&tconns, 1, __ATOMIC_RELEASE);
            cmap_insert(&ctx->cmap, conn);
            ctx->opened(conn, ctx->udata);
        }
        if (conn->bgctx) {
            // BGWORK(2)
            // The connection has been added back to the event loop, but it
            // needs to be attached and restated.
            ctx->qattachs[ctx->nqattachs++] = conn;
        } else if (conn->outlen > 0) {
            ctx->qouts[ctx->nqouts++] = conn;
        } else if (conn->closed) {
            ctx->qcloses[ctx->nqcloses++] = conn;
        } else {
            ctx->qreads[ctx->nqreads++] = conn;
        }
    }
}

inline
static void handle_read(ssize_t n, char *pkt, struct net_conn *conn,
    struct qthreadctx *ctx)
{
    assert(conn->outlen == 0);
    assert(conn->bgctx == 0);
    if (n <= 0) {
        if (n == 0 || errno != EAGAIN) {
            // read failed, close connection
            ctx->qcloses[ctx->nqcloses++] = conn;
            return;
        }
        assert(n == -1 && errno == EAGAIN);
        // even though there's an EAGAIN, still call the user data event
        // handler with an empty packet 
        n = 0;
    }
    pkt[n] = '\0';
    ctx->qins[ctx->nqins] = conn;
    ctx->qinpkts[ctx->nqins] = pkt;
    ctx->qinpktlens[ctx->nqins] = n;
    ctx->nqins++;
}

inline 
static void flush_conn(struct net_conn *conn, size_t written) {
    while (written < conn->outlen) {
        ssize_t n;
        if (conn->tls) {
            n = tls_write(conn->tls, conn->fd, conn->out+written, 
                conn->outlen-written);
        } else {
            n = write(conn->fd, conn->out+written, conn->outlen-written);
        }
        if (n == -1) {
            if (errno == EAGAIN) {
                continue;
            }
            conn->closed = true;
            break;
        }
        written += n;
    }
    // either everything was written or the socket is closed
    conn->outlen = 0;
}

inline
static void qattach(struct qthreadctx *ctx) {
    for (int i = 0; i < ctx->nqattachs; i++) {
        // BGWORK(3)
        // A bgworker has finished, make sure it's added back into the 
        // event loop in the correct state.
        struct net_conn *conn = ctx->qattachs[i];
        struct bgworkctx *bgctx = conn->bgctx;
        bgctx->done(conn, bgctx->udata);
        conn->bgctx = 0;
        assert(bgctx);
        xfree(bgctx);
        int ret = delwrite(conn->ctx->qfd, conn->fd);
        assert(ret == 0); (void)ret;
        ret = addread(conn->ctx->qfd, conn->fd);
        assert(ret == 0); (void)ret;
        flush_conn(conn, 0);
        if (conn->closed) {
            ctx->qcloses[ctx->nqcloses++] = conn;
        } else {
            ctx->qreads[ctx->nqreads++] = conn;
        }
    }
}

inline
static void qread(struct qthreadctx *ctx) {
    // Read incoming socket data
#ifndef NOURING
    if (ctx->uring && ctx->nqreads >= MINURINGEVENTS && ctx->ntlsconns == 0) {
        // read incoming using uring
        for (int i = 0; i < ctx->nqreads; i++) {
            struct net_conn *conn = ctx->qreads[i];
            char *pkt = ctx->inpkts+(i*PACKETSIZE);
            struct io_uring_sqe *sqe = io_uring_get_sqe(&ctx->ring);
            io_uring_prep_read(sqe, conn->fd, pkt, PACKETSIZE-1, 0);
        }
        int ret = io_uring_submit(&ctx->ring);
        if (ret < 0) {
            errno = -ret;
            perror("# io_uring_submit");
            abort();
        }
        assert(ret == ctx->nqreads);
        for (int i = 0; i < ctx->nqreads; i++) {
            struct io_uring_cqe *cqe;
            if (io_uring_wait_cqe(&ctx->ring, &cqe) < 0) {
                perror("# io_uring_wait_cqe");
                abort();
            }
            struct net_conn *conn = ctx->qreads[i];
            char *pkt = ctx->inpkts+(i*PACKETSIZE);
            ssize_t n = cqe->res;
            if (n < 0) {
                errno = -n;
                n = -1;
            }
            handle_read(n, pkt, conn, ctx);
            io_uring_cqe_seen(&ctx->ring, cqe);
        }
    } else {
#endif
        // read incoming data using standard syscalls.
        for (int i = 0; i < ctx->nqreads; i++) {
            struct net_conn *conn = ctx->qreads[i];
            char *pkt = ctx->inpkts+(i*PACKETSIZE);
            ssize_t n;
            if (conn->tls) {
                n = tls_read(conn->tls, conn->fd, pkt, PACKETSIZE-1);
            } else {
                n = read(conn->fd, pkt, PACKETSIZE-1);
            }
            handle_read(n, pkt, conn, ctx);
        }
#ifndef NOURING
    }
#endif
}


inline
static void qprocess(struct qthreadctx *ctx) {
    // process all new incoming data
    for (int i = 0; i < ctx->nqins; i++) {
        struct net_conn *conn = ctx->qins[i];
        char *p = ctx->qinpkts[i];
        int n = ctx->qinpktlens[i];
        ctx->data(conn, p, n, ctx->udata);
        sumstats(conn, ctx);
        if (conn->bgctx) {
            // BGWORK(1)
            // Connection entered background mode.
            // This means the connection is no longer in the event queue but
            // is still owned by this qthread. Once the bgwork is done the 
            // connection will be added back to the queue with addwrite.
        } else if (conn->outlen > 0) {
            ctx->qouts[ctx->nqouts++] = conn;
        } else if (conn->closed) {
            ctx->qcloses[ctx->nqcloses++] = conn;
        }
    }
}

inline
static void qprewrite(struct qthreadctx *ctx) {
    (void)ctx;
    // TODO: perform any prewrite operations
}

inline
static void qwrite(struct qthreadctx *ctx) {
    // Flush all outgoing socket data.
#ifndef NOURING
    if (ctx->uring && ctx->nqreads >= MINURINGEVENTS && ctx->ntlsconns == 0) {
        // write outgoing using uring
        for (int i = 0; i < ctx->nqouts; i++) {
            struct net_conn *conn = ctx->qouts[i];
            struct io_uring_sqe *sqe = io_uring_get_sqe(&ctx->ring);
            io_uring_prep_write(sqe, conn->fd, conn->out, conn->outlen, 0);
        }
        int ret = io_uring_submit(&ctx->ring);
        if (ret < 0) {
            errno = -ret;
            perror("# io_uring_submit");
            abort();
        }
        for (int i = 0; i < ctx->nqouts; i++) {
            struct io_uring_cqe *cqe;
            if (io_uring_wait_cqe(&ctx->ring, &cqe) < 0) {
                perror("# io_uring_wait_cqe");
                abort();
            }
            struct net_conn *conn = ctx->qouts[i];
            ssize_t n = cqe->res;
            if (n == -EAGAIN) {
                n = 0;
            }
            if (n < 0) {
                conn->closed = true;
            } else {
                // Any extra data must be flushed using syscall write.
                flush_conn(conn, n);
            }
            // Either everything was written or the socket is closed
            conn->outlen = 0;
            if (conn->closed) {
                ctx->qcloses[ctx->nqcloses++] = conn;
            }
            io_uring_cqe_seen(&ctx->ring, cqe);
        }
    } else {
#endif
        // Write data using write syscall
        for (int i = 0; i < ctx->nqouts; i++) {
            struct net_conn *conn = ctx->qouts[i];
            flush_conn(conn, 0);
            if (conn->closed) {
                ctx->qcloses[ctx->nqcloses++] = conn;
            }
        }
#ifndef NOURING
    }
#endif
}

inline
static void qclose(struct qthreadctx *ctx) {
    // Close all sockets that need to be closed
    for (int i = 0; i < ctx->nqcloses; i++) {
        struct net_conn *conn = ctx->qcloses[i];
        ctx->closed(conn, ctx->udata);
        if (conn->tls) {
            tls_close(conn->tls, conn->fd);
            ctx->ntlsconns--;
        } else {
            close(conn->fd);
        }
        cmap_delete(&ctx->cmap, conn);
        atomic_fetch_sub_explicit(&nconns, 1, __ATOMIC_RELEASE);
        atomic_fetch_sub_explicit(&ctx->nconns, 1, __ATOMIC_RELEASE);
        conn_free(conn);
    }
}

static void *qthread(void *arg) {
    struct qthreadctx *ctx = arg;
#ifndef NOURING
    if (ctx->uring) {
        if (io_uring_queue_init(ctx->queuesize, &ctx->ring, 0) < 0) {
            perror("# io_uring_queue_init");
            abort();
        }
    }
#endif
    // connection map
    memset(&ctx->cmap, 0, sizeof(struct cmap));
    ctx->cmap.nbuckets = 64;
    size_t size = ctx->cmap.nbuckets*sizeof(struct net_conn*);
    ctx->cmap.buckets = xmalloc(size);
    memset(ctx->cmap.buckets, 0, ctx->cmap.nbuckets*sizeof(struct net_conn*));

    ctx->events = xmalloc(sizeof(event_t)*ctx->queuesize);
    ctx->qreads = xmalloc(sizeof(struct net_conn*)*ctx->queuesize);
    ctx->inpkts = xmalloc(PACKETSIZE*ctx->queuesize);
    ctx->qins = xmalloc(sizeof(struct net_conn*)*ctx->queuesize);
    ctx->qinpkts = xmalloc(sizeof(char*)*ctx->queuesize);
    ctx->qinpktlens = xmalloc(sizeof(int)*ctx->queuesize);
    ctx->qcloses = xmalloc(sizeof(struct net_conn*)*ctx->queuesize);
    ctx->qouts = xmalloc(sizeof(struct net_conn*)*ctx->queuesize);
    ctx->qattachs = xmalloc(sizeof(struct net_conn*)*ctx->queuesize);

    while (1) {
        sumstats_global(ctx);
        ctx->nevents = getevents(ctx->qfd, ctx->events, ctx->queuesize, 1, 0);
        if (ctx->nevents <= 0) {
            if (ctx->nevents == -1 && errno != EINTR) {
                perror("# getevents");
                abort();
            }
            continue;
        }
        // reset, accept, attach, read, process, prewrite, write, close
        qreset(ctx);    // reset the step queues
        qaccept(ctx);   // accept incoming connections
        qattach(ctx);   // attach bg workers. uncommon
        qread(ctx);     // read from sockets
        qprocess(ctx);  // process new socket data
        qprewrite(ctx); // perform any prewrite operations, such as fsync
        qwrite(ctx);    // write to sockets
        qclose(ctx);    // close any sockets that need closing
    }
    return 0;
}

static int listen_tcp(const char *host, const char *port, bool reuseport, 
    int backlog)
{
    if (!port || !*port || strcmp(port, "0") == 0) {
        return 0;
    }
    int ret;
    host = host ? host : "127.0.0.1";
    port = port ? port : "0";
    struct addrinfo hints = { 0 }, *addrs;
    hints.ai_family = AF_UNSPEC; 
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    ret = getaddrinfo(host, port, &hints, &addrs);
    if (ret != 0) {
        fprintf(stderr, "# getaddrinfo: %s: %s:%s", gai_strerror(ret), host,
            port);
        abort();
    }
    struct addrinfo *ainfo = addrs;
    while (ainfo->ai_family != PF_INET) {
        ainfo = ainfo->ai_next;
    }
    assert(ainfo);
    int fd = socket(ainfo->ai_family, ainfo->ai_socktype, ainfo->ai_protocol);
    if (fd == -1) {
        perror("# socket(tcp)");
        abort();
    }
    if (reuseport) {
        ret = setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &(int){1}, 
            sizeof(int));
        if (ret == -1) {
            perror("# setsockopt(reuseport)");
            abort();
        }
    }
    ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &(int){1},sizeof(int));
    if (ret == -1) {
        perror("# setsockopt(reuseaddr)");
        abort();
    }
    ret = setnonblock(fd);
    if (ret == -1) {
        perror("# setnonblock");
        abort();
    }
    ret = bind(fd, ainfo->ai_addr, ainfo->ai_addrlen);
    if (ret == -1) {
        fprintf(stderr, "# bind(tcp): %s:%s", host, port);
        abort();
    }
    ret = listen(fd, backlog);
    if (ret == -1) {
        fprintf(stderr, "# listen(tcp): %s:%s", host, port);
        abort();
    }
    freeaddrinfo(addrs);
    return fd;
}

static int listen_unixsock(const char *unixsock, int backlog) {
    if (!unixsock || !*unixsock) {
        return 0;
    }
    struct sockaddr_un unaddr;
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("# socket(unix)");
        abort();
    }
    memset(&unaddr, 0, sizeof(struct sockaddr_un));
    unaddr.sun_family = AF_UNIX;
    strncpy(unaddr.sun_path, unixsock, sizeof(unaddr.sun_path) - 1);
    int ret = setnonblock(fd);
    if (ret == -1) {
        perror("# setnonblock");
        abort();
    }
    unlink(unixsock);
    ret = bind(fd, (struct sockaddr *)&unaddr, sizeof(struct sockaddr_un));
    if (ret == -1) {
        fprintf(stderr, "# bind(unix): %s", unixsock);
        abort();
    }
    ret = listen(fd, backlog);
    if (ret == -1) {
        fprintf(stderr, "# listen(unix): %s", unixsock);
        abort();
    }
    return fd;
}

static atomic_uintptr_t all_ctxs = 0;

// current connections
size_t net_nconns(void) {
    return atomic_load_explicit(&nconns, __ATOMIC_ACQUIRE);
}

// total connections ever
size_t net_tconns(void) {
    return atomic_load_explicit(&tconns, __ATOMIC_ACQUIRE);
}

// total rejected connections ever
size_t net_rconns(void) {
    return atomic_load_explicit(&rconns, __ATOMIC_ACQUIRE);
}

static void warmupunix(const char *unixsock, int nsocks) {
    if (!unixsock || !*unixsock) {
        return;
    }
    int *socks = xmalloc(nsocks*sizeof(int));
    memset(socks, 0, nsocks*sizeof(int));
    for (int i = 0; i < nsocks; i++) {
        socks[i] = socket(AF_UNIX, SOCK_STREAM, 0);
        if (socks[i] == -1) {
            socks[i] = 0;
            continue;
        }
        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(struct sockaddr_un));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, unixsock, sizeof(addr.sun_path) - 1);
        if (connect(socks[i], (struct sockaddr *)&addr, 
            sizeof(struct sockaddr_un)) == -1)
        {
            close(socks[i]);
            socks[i] = 0;
            continue;
        }
        ssize_t n = write(socks[i], "+PING\r\n", 7);
        if (n == -1) {
            close(socks[i]);
            socks[i] = 0;
            continue;
        }
    }
    int x = 0;
    for (int i = 0; i < nsocks; i++) {
        if (socks[i] > 0) {
            x++;
            close(socks[i]);
        }
    }
    if (verb > 1) {
        printf(". Warmup unix socket (%d/%d)\n", x, nsocks);
    }
    xfree(socks);
}


static void warmuptcp(const char *host, const char *port, int nsocks) {
    if (!port || !*port || strcmp(port, "0") == 0) {
        return;
    }
    int *socks = xmalloc(nsocks*sizeof(int));
    memset(socks, 0, nsocks*sizeof(int));
    for (int i = 0; i < nsocks; i++) {
        struct addrinfo hints, *res;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        int err = getaddrinfo(host, port, &hints, &res);
        if (err != 0) {
            continue;
        }
        socks[i] = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (socks[i] == -1) {
            freeaddrinfo(res);
            continue;
        }
        int ret = connect(socks[i], res->ai_addr, res->ai_addrlen);
        freeaddrinfo(res);
        if (ret == -1) {
            close(socks[i]);
            socks[i] = 0;
            continue;
        }
        ssize_t n = write(socks[i], "+PING\r\n", 7);
        if (n == -1) {
            close(socks[i]);
            socks[i] = 0;
            continue;
        }
    }
    int x = 0;
    for (int i = 0; i < nsocks; i++) {
        if (socks[i] > 0) {
            x++;
            close(socks[i]);
        }
    }
    if (verb > 1) {
        printf(". Warmup tcp (%d/%d)\n", x, nsocks);
    }
    xfree(socks);
}

static void *thwarmup(void *arg) {
    // Perform a warmup of the epoll queues and listeners by making a quick
    // connection to each.
    struct net_opts *opts = arg;
    warmupunix(opts->unixsock, opts->nthreads*2);
    warmuptcp(opts->host, opts->port, opts->nthreads*2);
    return 0;
}

void net_main(struct net_opts *opts) {
    (void)delread;
    int sfd[3] = {
        listen_tcp(opts->host, opts->port, opts->reuseport, opts->backlog),
        listen_unixsock(opts->unixsock, opts->backlog),
        listen_tcp(opts->host, opts->tlsport, opts->reuseport, opts->backlog),
    };
    if (!sfd[0] && !sfd[1] && !sfd[2]) {
        printf("# No listeners provided\n");
        abort();
    }
    opts->listening(opts->udata);
    struct qthreadctx *ctxs = xmalloc(sizeof(struct qthreadctx)*opts->nthreads);
    memset(ctxs, 0, sizeof(struct qthreadctx)*opts->nthreads);
    for (int i = 0; i < opts->nthreads; i++) {
        struct qthreadctx *ctx = &ctxs[i];
        ctx->nthreads = opts->nthreads;
        ctx->tcpnodelay = opts->tcpnodelay;
        ctx->keepalive = opts->keepalive;
        ctx->quickack = opts->quickack;
        ctx->uring = !opts->nouring;
        ctx->ctxs = ctxs;
        ctx->index = i;
        ctx->maxconns = opts->maxconns;
        ctx->sfd = sfd;
        ctx->data = opts->data;
        ctx->udata = opts->udata;
        ctx->opened = opts->opened;
        ctx->closed = opts->closed;
        ctx->qfd = evqueue();
        if (ctx->qfd == -1) {
            perror("# evqueue");
            abort();
        }
        atomic_init(&ctx->nconns, 0);
        for (int j = 0; j < 3; j++) {
            if (sfd[j]) {
                int ret = addread(ctx->qfd, sfd[j]);
                if (ret == -1) {
                    perror("# addread");
                    abort();
                }
            }
        }
        ctx->unixsock = opts->unixsock;
        ctx->queuesize = opts->queuesize;
    }
    atomic_store(&all_ctxs, (uintptr_t)(void*)ctxs);
    opts->ready(opts->udata);
    if (!opts->nowarmup) {
        pthread_t th;
        int ret = pthread_create(&th, 0, thwarmup, opts);
        if (ret != -1) {
            pthread_detach(th);
        }
    }
    for (int i = 0; i < opts->nthreads; i++) {
        struct qthreadctx *ctx = &ctxs[i];
        if (i == opts->nthreads-1) {
            qthread(ctx);
        } else {
            int ret = pthread_create(&ctx->th, 0, qthread, ctx);
            if (ret == -1) {
                perror("# pthread_create");
                abort();
            }
        }
    }
}

static void *bgwork(void *arg) {
    struct bgworkctx *bgctx = arg;
    bgctx->work(bgctx->udata);
    // We are not in the same thread context as the event loop that owns this
    // connection. Adding the writer to the queue will allow for the loop
    // thread to gracefully continue the operation and then call the 'done'
    // callback.
    int ret = addwrite(bgctx->conn->ctx->qfd, bgctx->conn->fd);
    assert(ret == 0); (void)ret;
    return 0;
}

// net_conn_bgwork processes work in a background thread.
// When work is finished, the done function is called.
// It's not safe to use the conn type in the work function.
bool net_conn_bgwork(struct net_conn *conn, void (*work)(void *udata), 
    void (*done)(struct net_conn *conn, void *udata), void *udata)
{
    if (conn->bgctx || conn->closed) {
        return false;
    }
    struct qthreadctx *ctx = conn->ctx;
    int ret = delread(ctx->qfd, conn->fd);
    assert(ret == 0); (void)ret;
    conn->bgctx = xmalloc(sizeof(struct bgworkctx));
    memset(conn->bgctx, 0, sizeof(struct bgworkctx));
    conn->bgctx->conn = conn;
    conn->bgctx->done = done;
    conn->bgctx->work = work;
    conn->bgctx->udata = udata;
    pthread_t th;
    if (pthread_create(&th, 0, bgwork, conn->bgctx) == -1) {
        // Failed to create thread. Revert and return false.
        ret = addread(ctx->qfd, conn->fd);
        assert(ret == 0);
        xfree(conn->bgctx);
        conn->bgctx = 0;
        return false;
    } else {
        pthread_detach(th);
    }
    return true;
}

bool net_conn_bgworking(struct net_conn *conn) {
    return conn->bgctx != 0;
}

void net_stat_cmd_get_incr(struct net_conn *conn) {
    conn->stat_cmd_get++;
}

void net_stat_cmd_set_incr(struct net_conn *conn) {
    conn->stat_cmd_set++;
}

void net_stat_get_hits_incr(struct net_conn *conn) {
    conn->stat_get_hits++;
}

void net_stat_get_misses_incr(struct net_conn *conn) {
    conn->stat_get_misses++;
}

bool net_conn_istls(struct net_conn *conn) {
    return conn->tls != 0;
}
