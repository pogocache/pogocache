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
// Unit xmalloc.c is the primary allocator interface. The xmalloc/xfree
// functions should be used instead of malloc/free.
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdatomic.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "sys.h"
#include "xmalloc.h"

#if defined(__linux__) && defined(__GLIBC__)
#include <malloc.h>
#define HAS_MALLOC_H
#endif

#ifndef NOMIMALLOC
#include "../deps/mimalloc/include/mimalloc.h"
#endif

#ifndef NOJEMALLOC
#include "../deps/jemalloc/include/jemalloc/jemalloc.h"
#endif

// from main.c
extern const int useallocator;
extern const bool usetrackallocs;

#ifdef NOTRACKALLOCS
#define add_alloc()
#define sub_alloc()
size_t xallocs(void) {
    return 0;
}
#else
static atomic_int_fast64_t nallocs = 0;

size_t xallocs(void) {
    if (usetrackallocs) {
        return atomic_load(&nallocs);
    } else {
        return 0;
    }
}

static void add_alloc(void) {
    if (usetrackallocs) {
        atomic_fetch_add_explicit(&nallocs, 1, __ATOMIC_RELAXED);
    }
}

static void sub_alloc(void) {
    if (usetrackallocs) {
        atomic_fetch_sub_explicit(&nallocs, 1, __ATOMIC_RELAXED);
    }
}
#endif

void xmalloc_init(int nthreads) {
    (void)nthreads;
    switch (useallocator) {
#ifndef NOMIMALLOC
    case ALLOCATOR_MIMALLOC:
        // initialization for mimalloc
        break;
#endif
#ifndef NOJEMALLOC
    case ALLOCATOR_JEMALLOC:
        // initialization for jemalloc
        break;
#endif
    default:
        // initialization for stock allocator
        break;
    }
}

static void check_ptr(void *ptr) {
    if (!ptr) {
        fprintf(stderr, "# %s\n", strerror(ENOMEM));
        abort();
    }
}

static void *malloc0(size_t size) {
    switch (useallocator) {
#ifndef NOMIMALLOC
    case ALLOCATOR_MIMALLOC:
        return mi_malloc(size);
#endif
#ifndef NOJEMALLOC
    case ALLOCATOR_JEMALLOC:
        return je_malloc(size);
#endif
    default:
        return malloc(size);
    }
}

static void *realloc0(void *ptr, size_t size) {
    switch (useallocator) {
#ifndef NOMIMALLOC
    case ALLOCATOR_MIMALLOC:
        return mi_realloc(ptr, size);
#endif
#ifndef NOJEMALLOC
    case ALLOCATOR_JEMALLOC:
        return je_realloc(ptr, size);
#endif
    default:
        return realloc(ptr, size);
    }
}

static void free0(void *ptr) {
    switch (useallocator) {
#ifndef NOMIMALLOC
    case ALLOCATOR_MIMALLOC:
        mi_free(ptr);
        break;
#endif
#ifndef NOJEMALLOC
    case ALLOCATOR_JEMALLOC:
        je_free(ptr);
        break;
#endif
    default:
        free(ptr);
        break;
    }
}

void *xmalloc(size_t size) {
    void *ptr = malloc0(size);
    check_ptr(ptr);
    add_alloc();
    return ptr;
}

void *xrealloc(void *ptr, size_t size) {
    if (!ptr) {
        return xmalloc(size);
    }
    ptr = realloc0(ptr, size);
    check_ptr(ptr);
    return ptr;
}

void xfree(void *ptr) {
    if (!ptr) {
        return;
    }
    free0(ptr);
    sub_alloc();
}

void xpurge(void) {
    // Releases unused heap memory to OS
    switch (useallocator) {
#ifndef NOMIMALLOC
    case ALLOCATOR_MIMALLOC:
        mi_collect(true);
        break;
#endif
#ifndef NOJEMALLOC
    case ALLOCATOR_JEMALLOC:
        break;
#endif
    default:
#ifdef HAS_MALLOC_H
        malloc_trim(0);
#endif
        break;
    }
}


size_t xrss(void) {
    struct sys_meminfo info;
    sys_getmeminfo(&info);
    return info.rss;
}
