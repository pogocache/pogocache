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

void *xmalloc(size_t size) {
    void *ptr = malloc(size);
    check_ptr(ptr);
    add_alloc();
    return ptr;
}

void *xrealloc(void *ptr, size_t size) {
    if (!ptr) {
        return xmalloc(size);
    }
    ptr = realloc(ptr, size);
    check_ptr(ptr);
    return ptr;
}

void xfree(void *ptr) {
    if (!ptr) {
        return;
    }
    free(ptr);
    sub_alloc();
}

void xpurge(void) {
#ifdef HAS_MALLOC_H
    // Releases unused heap memory to OS
    malloc_trim(0);
#endif
}
