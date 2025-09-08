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
void *mi_malloc(size_t);
void *mi_realloc(void*,size_t);
void mi_free(void*);
#else
#define mi_malloc malloc
#define mi_realloc realloc
#define mi_free free
#endif

#ifndef NOJEMALLOC
void *je_malloc(size_t);
void *je_realloc(void*,size_t);
void je_free(void*);
#else
#define je_malloc malloc
#define je_realloc realloc
#define je_free free
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

static void check_ptr(void *ptr) {
    if (!ptr) {
        fprintf(stderr, "# %s\n", strerror(ENOMEM));
        abort();
    }
}

static void *malloc0(size_t size) {
    switch (useallocator) {
    case ALLOCATOR_JEMALLOC:
        return je_malloc(size);
    case ALLOCATOR_MIMALLOC:
        return mi_malloc(size);
    default:
        return malloc(size);
    }
}

static void *realloc0(void *ptr, size_t size) {
    switch (useallocator) {
    case ALLOCATOR_JEMALLOC:
        return je_realloc(ptr, size);
    case ALLOCATOR_MIMALLOC:
        return mi_realloc(ptr, size);
    default:
        return realloc(ptr, size);
    }
}


static void free0(void *ptr) {
    switch (useallocator) {
    case ALLOCATOR_JEMALLOC:
        je_free(ptr);
        break;
    case ALLOCATOR_MIMALLOC:
        mi_free(ptr);
        break;
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
#ifdef HAS_MALLOC_H
    // Releases unused heap memory to OS
    malloc_trim(0);
#endif
}
