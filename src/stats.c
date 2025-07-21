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
// Unit stats.c tracks various stats. Mostly for the memcache protocol.
#include <stdatomic.h>
#include "stats.h"

static atomic_uint_fast64_t g_stat_cmd_flush = 0;
static atomic_uint_fast64_t g_stat_cmd_touch = 0;
static atomic_uint_fast64_t g_stat_cmd_meta = 0;
static atomic_uint_fast64_t g_stat_get_expired = 0;
static atomic_uint_fast64_t g_stat_get_flushed = 0;
static atomic_uint_fast64_t g_stat_delete_misses = 0;
static atomic_uint_fast64_t g_stat_delete_hits = 0;
static atomic_uint_fast64_t g_stat_incr_misses = 0;
static atomic_uint_fast64_t g_stat_incr_hits = 0;
static atomic_uint_fast64_t g_stat_decr_misses = 0;
static atomic_uint_fast64_t g_stat_decr_hits = 0;
static atomic_uint_fast64_t g_stat_cas_misses = 0;
static atomic_uint_fast64_t g_stat_cas_hits = 0;
static atomic_uint_fast64_t g_stat_cas_badval = 0;
static atomic_uint_fast64_t g_stat_touch_hits = 0;
static atomic_uint_fast64_t g_stat_touch_misses = 0;
static atomic_uint_fast64_t g_stat_store_too_large = 0;
static atomic_uint_fast64_t g_stat_store_no_memory = 0;
static atomic_uint_fast64_t g_stat_auth_cmds = 0;
static atomic_uint_fast64_t g_stat_auth_errors = 0;

void stat_cmd_flush_incr(struct conn *conn) {
    (void)conn;
    atomic_fetch_add_explicit(&g_stat_cmd_flush, 1, __ATOMIC_RELAXED);
}

void stat_cmd_touch_incr(struct conn *conn) {
    (void)conn;
    atomic_fetch_add_explicit(&g_stat_cmd_touch, 1, __ATOMIC_RELAXED);
}

void stat_cmd_meta_incr(struct conn *conn) {
    (void)conn;
    atomic_fetch_add_explicit(&g_stat_cmd_meta, 1, __ATOMIC_RELAXED);
}

void stat_get_expired_incr(struct conn *conn) {
    (void)conn;
    atomic_fetch_add_explicit(&g_stat_get_expired, 1, __ATOMIC_RELAXED);
}

void stat_get_flushed_incr(struct conn *conn) {
    (void)conn;
    atomic_fetch_add_explicit(&g_stat_get_flushed, 1, __ATOMIC_RELAXED);
}

void stat_delete_misses_incr(struct conn *conn) {
    (void)conn;
    atomic_fetch_add_explicit(&g_stat_delete_misses, 1, __ATOMIC_RELAXED);
}

void stat_delete_hits_incr(struct conn *conn) {
    (void)conn;
    atomic_fetch_add_explicit(&g_stat_delete_hits, 1, __ATOMIC_RELAXED);
}

void stat_incr_misses_incr(struct conn *conn) {
    (void)conn;
    atomic_fetch_add_explicit(&g_stat_incr_misses, 1, __ATOMIC_RELAXED);
}

void stat_incr_hits_incr(struct conn *conn) {
    (void)conn;
    atomic_fetch_add_explicit(&g_stat_incr_hits, 1, __ATOMIC_RELAXED);
}

void stat_decr_misses_incr(struct conn *conn) {
    (void)conn;
    atomic_fetch_add_explicit(&g_stat_decr_misses, 1, __ATOMIC_RELAXED);
}

void stat_decr_hits_incr(struct conn *conn) {
    (void)conn;
    atomic_fetch_add_explicit(&g_stat_decr_hits, 1, __ATOMIC_RELAXED);
}

void stat_cas_misses_incr(struct conn *conn) {
    (void)conn;
    atomic_fetch_add_explicit(&g_stat_cas_misses, 1, __ATOMIC_RELAXED);
}

void stat_cas_hits_incr(struct conn *conn) {
    (void)conn;
    atomic_fetch_add_explicit(&g_stat_cas_hits, 1, __ATOMIC_RELAXED);
}

void stat_cas_badval_incr(struct conn *conn) {
    (void)conn;
    atomic_fetch_add_explicit(&g_stat_cas_badval, 1, __ATOMIC_RELAXED);
}

void stat_touch_hits_incr(struct conn *conn) {
    (void)conn;
    atomic_fetch_add_explicit(&g_stat_touch_hits, 1, __ATOMIC_RELAXED);
}

void stat_touch_misses_incr(struct conn *conn) {
    (void)conn;
    atomic_fetch_add_explicit(&g_stat_touch_misses, 1, __ATOMIC_RELAXED);
}

void stat_store_too_large_incr(struct conn *conn) {
    (void)conn;
    atomic_fetch_add_explicit(&g_stat_store_too_large, 1, __ATOMIC_RELAXED);
}

void stat_store_no_memory_incr(struct conn *conn) {
    (void)conn;
    atomic_fetch_add_explicit(&g_stat_store_no_memory, 1, __ATOMIC_RELAXED);
}

void stat_auth_cmds_incr(struct conn *conn) {
    (void)conn;
    atomic_fetch_add_explicit(&g_stat_auth_cmds, 1, __ATOMIC_RELAXED);
}

void stat_auth_errors_incr(struct conn *conn) {
    (void)conn;
    atomic_fetch_add_explicit(&g_stat_auth_errors, 1, __ATOMIC_RELAXED);
}

uint64_t stat_cmd_flush(void) {
    return atomic_load_explicit(&g_stat_cmd_flush, __ATOMIC_RELAXED);
}

uint64_t stat_cmd_touch(void) {
    return atomic_load_explicit(&g_stat_cmd_touch, __ATOMIC_RELAXED);
}

uint64_t stat_cmd_meta(void) {
    return atomic_load_explicit(&g_stat_cmd_meta, __ATOMIC_RELAXED);
}

uint64_t stat_get_expired(void) {
    return atomic_load_explicit(&g_stat_get_expired, __ATOMIC_RELAXED);
}

uint64_t stat_get_flushed(void) {
    return atomic_load_explicit(&g_stat_get_flushed, __ATOMIC_RELAXED);
}

uint64_t stat_delete_misses(void) {
    return atomic_load_explicit(&g_stat_delete_misses, __ATOMIC_RELAXED);
}

uint64_t stat_delete_hits(void) {
    return atomic_load_explicit(&g_stat_delete_hits, __ATOMIC_RELAXED);
}

uint64_t stat_incr_misses(void) {
    return atomic_load_explicit(&g_stat_incr_misses, __ATOMIC_RELAXED);
}

uint64_t stat_incr_hits(void) {
    return atomic_load_explicit(&g_stat_incr_hits, __ATOMIC_RELAXED);
}

uint64_t stat_decr_misses(void) {
    return atomic_load_explicit(&g_stat_decr_misses, __ATOMIC_RELAXED);
}

uint64_t stat_decr_hits(void) {
    return atomic_load_explicit(&g_stat_decr_hits, __ATOMIC_RELAXED);
}

uint64_t stat_cas_misses(void) {
    return atomic_load_explicit(&g_stat_cas_misses, __ATOMIC_RELAXED);
}

uint64_t stat_cas_hits(void) {
    return atomic_load_explicit(&g_stat_cas_hits, __ATOMIC_RELAXED);
}

uint64_t stat_cas_badval(void) {
    return atomic_load_explicit(&g_stat_cas_badval, __ATOMIC_RELAXED);
}

uint64_t stat_touch_hits(void) {
    return atomic_load_explicit(&g_stat_touch_hits, __ATOMIC_RELAXED);
}

uint64_t stat_touch_misses(void) {
    return atomic_load_explicit(&g_stat_touch_misses, __ATOMIC_RELAXED);
}

uint64_t stat_store_too_large(void) {
    return atomic_load_explicit(&g_stat_store_too_large, __ATOMIC_RELAXED);
}

uint64_t stat_store_no_memory(void) {
    return atomic_load_explicit(&g_stat_store_no_memory, __ATOMIC_RELAXED);
}

uint64_t stat_auth_cmds(void) {
    return atomic_load_explicit(&g_stat_auth_cmds, __ATOMIC_RELAXED);
}

uint64_t stat_auth_errors(void) {
    return atomic_load_explicit(&g_stat_auth_errors, __ATOMIC_RELAXED);
}


