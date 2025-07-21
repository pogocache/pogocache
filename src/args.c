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
// Unit args.c provides functions for managing command arguments
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "args.h"
#include "xmalloc.h"
#include "util.h"

const char *args_at(struct args *args, int idx, size_t *len) {
    *len = args->bufs[idx].len;
    return args->bufs[idx].data;
}

int args_count(struct args *args) {
    return args->len;
}

bool args_eq(struct args *args, int index, const char *str) {
    if ((size_t)index >= args->len) {
        return false;
    }
    size_t alen = args->bufs[index].len;
    const char *arg = args->bufs[index].data;
    size_t slen = strlen(str); 
    if (alen != slen) {
        return false;
    }
    for (size_t i = 0; i < slen ; i++) {
        if (tolower(str[i]) != tolower(arg[i])) {
            return false;
        }
    }
    return true;
}

void args_append(struct args *args, const char *data, size_t len,
    bool zerocopy)
{
#ifdef NOZEROCOPY
    zerocopy = 0;
#endif
    if (args->len == args->cap) {
        args->cap = args->cap == 0 ? 4 : args->cap*2;
        args->bufs = xrealloc(args->bufs, args->cap * sizeof(struct buf));
        memset(&args->bufs[args->len], 0, (args->cap-args->len) * 
            sizeof(struct buf));
    }
    if (zerocopy) {
        buf_clear(&args->bufs[args->len]);
        args->bufs[args->len].len = len;
        args->bufs[args->len].data = (char*)data;
    } else {
        args->bufs[args->len].len = 0;
        buf_append(&args->bufs[args->len], data, len);
    }
    if (args->len == 0) {
        args->zerocopy = zerocopy;
    } else {
        args->zerocopy = args->zerocopy && zerocopy;
    }
    args->len++;
}

void args_clear(struct args *args) {
    if (!args->zerocopy) {
        for (size_t i = 0 ; i < args->len; i++) {
            buf_clear(&args->bufs[i]);
        }
    }
    args->len = 0;
}

void args_free(struct args *args) {
    args_clear(args);
    xfree(args->bufs);
}

void args_print(struct args *args) {
    printf(". ");
    for (size_t i = 0; i < args->len; i++) {
        char *buf = args->bufs[i].data;
        int len =  args->bufs[i].len;
        printf("["); 
        binprint(buf, len);
        printf("] ");
    }
    printf("\n");
}

// remove the first item
void args_remove_first(struct args *args) {
    if (args->len > 0) {
        buf_clear(&args->bufs[0]);
        for (size_t i = 1; i < args->len; i++) {
            args->bufs[i-1] = args->bufs[i];
        }
        args->len--;
    }
}
