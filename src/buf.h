// https://github.com/tidwall/pogocache
//
// Copyright 2025 Polypoint Labs, LLC. All rights reserved.
// This file is part of the Pogocache project.
// Use of this source code is governed by the AGPL that can be found in
// the LICENSE file.
//
// For alternative licensing options or general questions, please contact
// us at licensing@polypointlabs.com.
#ifndef BUF_H
#define BUF_H

#include <stddef.h>
#include <stdint.h>

struct buf {
    char *data;
    size_t len;
    size_t cap;
};

void buf_ensure(struct buf *buf, size_t len);
void buf_append(struct buf *buf, const void *data, size_t len);
void buf_append_byte(struct buf *buf, char byte);
void buf_clear(struct buf *buf);
void buf_append_uvarint(struct buf *buf, uint64_t x);
void buf_append_varint(struct buf *buf, int64_t x);

#endif
