// https://github.com/tidwall/pogocache
//
// Copyright 2025 Polypoint Labs, LLC. All rights reserved.
// This file is part of the Pogocache project.
// Use of this source code is governed by the AGPL that can be found in
// the LICENSE file.
//
// For alternative licensing options or general questions, please contact
// us at licensing@polypointlabs.com.
#ifndef POGOCACHE_H
#define POGOCACHE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Return values for pogocache operations
#define POGOCACHE_INSERTED 1
#define POGOCACHE_REPLACED 2
#define POGOCACHE_FOUND    3
#define POGOCACHE_NOTFOUND 4
#define POGOCACHE_DELETED  5
#define POGOCACHE_FINISHED 6
#define POGOCACHE_CANCELED 7
#define POGOCACHE_NOMEM    8

// Helper constants for ttls and expiration timestamps
#define POGOCACHE_NANOSECOND  INT64_C(1)
#define POGOCACHE_MICROSECOND INT64_C(1000)
#define POGOCACHE_MILLISECOND INT64_C(1000000)
#define POGOCACHE_SECOND      INT64_C(1000000000)
#define POGOCACHE_MINUTE      INT64_C(60000000000)
#define POGOCACHE_HOUR        INT64_C(3600000000000)

// Return values for the pogocache_iter_opts.entry callback
#define POGOCACHE_ITER_CONTINUE 0 // continue iterating
#define POGOCACHE_ITER_STOP     1 // stop iterating
#define POGOCACHE_ITER_DELETE   2 // delete current entry

// Reasons for eviction, param for the pogocache_opts.evicted callback
#define POGOCACHE_REASON_EXPIRED 1 // entry ttl has elapsed.
#define POGOCACHE_REASON_LOWMEM  2 // system is low on memory.
#define POGOCACHE_REASON_CLEARED 3 // pogocache_clear called.

struct pogocache_opts {
    void *(*malloc)(size_t);      // use a custom malloc function
    void (*free)(void*);          // use a custom free function
    void (*yield)(void *udata);   // contention yielder (default: no yielding)
    // The 'evicted' callback is called for every entry has been evicted due
    // to expiration, low memory, or when the cache is cleared. Check the 
    // 'reason' param for why the entry was evicted.
    void (*evicted)(int shard, int reason, int64_t time, const void *key,
        size_t keylen, const void *value, size_t valuelen, int64_t expires,
        uint32_t flags, uint64_t cas, void *udata);
    void *udata;         // user data for above callbacks
    // functionality options
    bool usecas;         // enable the compare-and-store operation
    bool nosixpack;      // disable sixpack key compression
    bool noevict;        // disable all eviction
    bool allowshrink;    // allow hashmap shrinking
    bool usethreadbatch; // use a thread local batch (non-reentrant)
    int nshards;         // default 65536
    int loadfactor;      // default 75%
    uint64_t seed;       // custom hash seed, default zero
};

struct pogocache_store_opts {
    int64_t time;    // current time (default: use internal monotonic clock)
    int64_t expires; // expiration time, nanoseconds, default: no expiration
    int64_t ttl;     // time-to-live, nanoseconds, default: no expiration
    uint64_t cas;    // CAS value (default: auto selected)
    uint32_t flags;  // 
    bool keepttl;    // 
    bool casop;      // perform the CAS operation (default: no)
    bool nx;         // 
    bool xx;         // 
    bool lowmem;     // tells the operation that the system is low on memory
    // The 'entry' callback returns the value of the old entry about to be
    // replaced by the new entry. This give the caller a chance to take a peek
    // at the entry before it gets replaced. Return true to store the new entry
    // and false to keep the old entry.
    bool (*entry)(int shard, int64_t time, const void *key, size_t keylen,
        const void *value, size_t valuelen, int64_t expires, uint32_t flags,
        uint64_t cas, void *udata);
    void *udata;
};

struct pogocache_update {
    const void *value;
    size_t valuelen;
    uint32_t flags;
    int64_t expires;
};

struct pogocache_load_opts {
    int64_t time;       // current time (default: use internal monotonic clock)
    bool notouch;       // do not update lru
    // The 'entry' callback return the value of the entry. This is required to
    // retreive the value of the current entry.
    void (*entry)(int shard, int64_t time, const void *key, size_t keylen,
        const void *value, size_t valuelen, int64_t expires, uint32_t flags,
        uint64_t cas, struct pogocache_update **update, void *udata);
    void *udata;
};

struct pogocache_delete_opts {
    int64_t time;       // current time (default: use internal monotonic clock)
    // The 'entry' callback is called before deleting the entry. It provides
    // all the existing entry value data.
    // Return true to delete the entry or false to cancel the delete and keep
    // the existing entry.
    bool (*entry)(int shard, int64_t time, const void *key, size_t keylen,
        const void *value, size_t valuelen, int64_t expires, uint32_t flags,
        uint64_t cas, void *udata);
    void *udata;
};

struct pogocache_iter_opts {
    int64_t time;       // current time (default: use internal monotonic clock)
    bool oneshard;      // only iter over one shard (default: all shards)
    int oneshardidx;    // index of one shard iteration, if oneshard is true. 
    // The 'entry' callback is called for each entry in the cache.
    // Return POGOCACHE_ITER_NEXT to continue iterating
    // Return POGOCACHE_ITER_STOP to stop iterating
    // Return POGOCACHE_ITER_DELETE to delete the entry and continue iterating.
    // It's allowed to return POGOCACHE_ITER_DELETE|POGOCACHE_ITER_STOP to
    // delete the entry and stop iterating.
    int (*entry)(int shard, int64_t time, const void *key, size_t keylen,
        const void *value, size_t valuelen, int64_t expires, uint32_t flags,
        uint64_t cas, void *udata);
    void *udata;
};

struct pogocache_count_opts {
    int64_t time;       // current time (default: use internal monotonic clock)
    bool oneshard;      // only count one shard (default: all shards)
    int oneshardidx;    // index of one shard count, if oneshard is true.
};

struct pogocache_total_opts {
    int64_t time;       // current time (default: use internal monotonic clock)
    bool oneshard;      // only count one shard (default: all shards)
    int oneshardidx;    // index of one shard count, if oneshard is true.
};

struct pogocache_size_opts {
    int64_t time;       // current time (default: use internal monotonic clock)
    bool oneshard;      // only count one shard (default: all shards)
    int oneshardidx;    // index of one shard count, if oneshard is true.
    bool entriesonly;   // do not include the structure size.
};

struct pogocache_sweep_opts {
    int64_t time;       // current time (default: use internal monotonic clock)
    bool oneshard;      // only sweep one shard (default: all shards)
    int oneshardidx;    // index of one shard to sweep, if oneshard is true.
};

struct pogocache_clear_opts {
    int64_t time;       // current time (default: use internal monotonic clock)
    bool oneshard;      // only clear one shard (default: all shards)
    int oneshardidx;    // index of one shard to clear, if oneshard is true.
};

struct pogocache_sweep_poll_opts {
    int64_t time;  // current time (default: use internal monotonic clock)
    int pollsize;  // number of entries to poll (default: 20)
};

struct pogocache;

// initialize/destroy
struct pogocache *pogocache_new(struct pogocache_opts *opts);
void pogocache_free(struct pogocache *cache);

// batching
struct pogocache *pogocache_begin(struct pogocache *cache);
void pogocache_end(struct pogocache *batch);

// key operations
int pogocache_delete(struct pogocache *cache, const void *key, size_t keylen, 
    struct pogocache_delete_opts *dopts);
int pogocache_store(struct pogocache *cache, const void *key, size_t keylen, 
    const void *value, size_t valuelen, struct pogocache_store_opts *opts);
int pogocache_load(struct pogocache *cache, const void *key, size_t keylen, 
    struct pogocache_load_opts *opts);

// scan operations
int pogocache_iter(struct pogocache *cache, struct pogocache_iter_opts *opts);
void pogocache_sweep(struct pogocache *cache, size_t *swept, size_t *kept, 
    struct pogocache_sweep_opts *opts);
void pogocache_clear(struct pogocache *cache,
    struct pogocache_clear_opts *opts);
double pogocache_sweep_poll(struct pogocache *cache,
    struct pogocache_sweep_poll_opts *opts);

// stat operations
size_t pogocache_count(struct pogocache *cache,
    struct pogocache_count_opts *opts);
uint64_t pogocache_total(struct pogocache *cache,
    struct pogocache_total_opts *opts);
size_t pogocache_size(struct pogocache *cache,
    struct pogocache_size_opts *opts);

// utilities
int pogocache_nshards(struct pogocache *cache);
int64_t pogocache_now(void);

#endif
