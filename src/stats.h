// https://github.com/tidwall/pogocache
//
// Copyright 2025 Polypoint Labs, LLC. All rights reserved.
// This file is part of the Pogocache project.
// Use of this source code is governed by the AGPL that can be found in
// the LICENSE file.
//
// For alternative licensing options or general questions, please contact
// us at licensing@polypointlabs.com.
#ifndef STATS_H
#define STATS_H

#include <stdint.h>
#include "conn.h"

void stat_cmd_flush_incr(struct conn *conn);
void stat_cmd_touch_incr(struct conn *conn);
void stat_cmd_meta_incr(struct conn *conn);
void stat_get_expired_incr(struct conn *conn);
void stat_get_flushed_incr(struct conn *conn);
void stat_delete_misses_incr(struct conn *conn);
void stat_delete_hits_incr(struct conn *conn);
void stat_incr_misses_incr(struct conn *conn);
void stat_incr_hits_incr(struct conn *conn);
void stat_decr_misses_incr(struct conn *conn);
void stat_decr_hits_incr(struct conn *conn);
void stat_cas_misses_incr(struct conn *conn);
void stat_cas_hits_incr(struct conn *conn);
void stat_cas_badval_incr(struct conn *conn);
void stat_touch_hits_incr(struct conn *conn);
void stat_touch_misses_incr(struct conn *conn);
void stat_store_too_large_incr(struct conn *conn);
void stat_store_no_memory_incr(struct conn *conn);
void stat_auth_cmds_incr(struct conn *conn);
void stat_auth_errors_incr(struct conn *conn);

uint64_t stat_cmd_flush(void);
uint64_t stat_cmd_touch(void);
uint64_t stat_cmd_meta(void);
uint64_t stat_get_expired(void);
uint64_t stat_get_flushed(void);
uint64_t stat_delete_misses(void);
uint64_t stat_delete_hits(void);
uint64_t stat_incr_misses(void);
uint64_t stat_incr_hits(void);
uint64_t stat_decr_misses(void);
uint64_t stat_decr_hits(void);
uint64_t stat_cas_misses(void);
uint64_t stat_cas_hits(void);
uint64_t stat_cas_badval(void);
uint64_t stat_touch_hits(void);
uint64_t stat_touch_misses(void);
uint64_t stat_store_too_large(void);
uint64_t stat_store_no_memory(void);
uint64_t stat_auth_cmds(void);
uint64_t stat_auth_errors(void);




#endif
