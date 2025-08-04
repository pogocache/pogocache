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
// Unit buf.c is a simple interface for creating byte buffers
#include <string.h>
#include "xmalloc.h"
#include "util.h"
#include "buf.h"

void buf_ensure(struct buf *buf, size_t len) {
    if (buf->len+len > buf->cap) {
        size_t oldcap = buf->cap;
        size_t newcap = buf->cap;
        if (oldcap == 0) {
            buf->data = 0;
            newcap = 16;
        } else {
            newcap *= 2;
        }
        while (buf->len+len > newcap) {
            newcap *= 2;
        }
        buf->data = xrealloc(buf->data, newcap);
        buf->cap = newcap;
    }
}

void buf_append(struct buf *buf, const void *data, size_t len){
    if (len > 0) {
        buf_ensure(buf, len);
        memcpy(buf->data+buf->len, data, len);
        buf->len += len;
    }
}

void buf_append_byte(struct buf *buf, char byte) {
    if (buf->len < buf->cap) {
        buf->data[buf->len++] = byte;
    } else {
        buf_append(buf, &byte, 1);
    }
}

void buf_clear(struct buf *buf) {
    // No capacity means this buffer is owned somewhere else and we 
    // must not free the data.
    if (buf->cap) {
        xfree(buf->data);
    }
    memset(buf, 0, sizeof(struct buf));
}

void buf_append_uvarint(struct buf *buf, uint64_t x) {
    buf_ensure(buf, 10);
    int n = varint_write_u64(buf->data+buf->len, x);
    buf->len += n;
}

void buf_append_varint(struct buf *buf, int64_t x) {
    buf_ensure(buf, 10);
    int n = varint_write_i64(buf->data+buf->len, x);
    buf->len += n;
}
