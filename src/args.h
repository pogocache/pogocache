// https://github.com/tidwall/pogocache
//
// Copyright 2025 Polypoint Labs, LLC. All rights reserved.
// This file is part of the Pogocache project.
// Use of this source code is governed by the AGPL that can be found in
// the LICENSE file.
//
// For alternative licensing options or general questions, please contact
// us at licensing@polypointlabs.com.
#ifndef ARGS_H
#define ARGS_H

#include <stddef.h>
#include <stdbool.h>
#include "buf.h"

struct args {
    bool zerocopy;    // all args are zerocopy
    struct buf *bufs;
    size_t len;
    size_t cap;
};

const char *args_at(struct args *args, int index, size_t *len);
int args_count(struct args *args);
bool args_eq(struct args *args, int index, const char *cmd);
void args_append(struct args *args, const char *data, size_t len,
    bool zerocopy);
void args_print(struct args *args);
void args_free(struct args *args);
void args_clear(struct args *args);

void args_remove_first(struct args *args);

#endif
