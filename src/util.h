// https://github.com/tidwall/pogocache
//
// Copyright 2025 Polypoint Labs, LLC. All rights reserved.
// This file is part of the Pogocache project.
// Use of this source code is governed by the AGPL that can be found in
// the LICENSE file.
//
// For alternative licensing options or general questions, please contact
// us at licensing@polypointlabs.com.
#ifndef UTIL_H
#define UTIL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include "args.h"

#define NANOSECOND  INT64_C(1)
#define MICROSECOND INT64_C(1000)
#define MILLISECOND INT64_C(1000000)
#define SECOND      INT64_C(1000000000)
#define MINUTE      INT64_C(60000000000)
#define HOUR        INT64_C(3600000000000)

const char *memstr(double size, char buf[64]);
const char *memstr_long(double size, char buf[64]);
int64_t int64_mul_clamp(int64_t a, int64_t b);
int64_t int64_add_clamp(int64_t a, int64_t b);
bool argi64(struct args *args, int idx, int64_t *x);
bool argu64(struct args *args, int idx, uint64_t *x);
bool argeq(struct args *args, int idx, const char *cstr);
bool argeq_bytes(const void *data, size_t strlen, const char *cstr);
int varint_write_u64(void *data, uint64_t x);
int varint_read_u64(const void *data, size_t len, uint64_t *x);
int varint_write_i64(void *data, int64_t x);
int varint_read_i64(const void *data, size_t len, int64_t *x);
size_t u64toa(uint64_t x, uint8_t *data);
size_t i64toa(int64_t x, uint8_t *data);

bool parse_i64(const char *data, size_t len, int64_t *x);
bool parse_u64(const char *data, size_t len, uint64_t *x);

uint64_t rand_next(uint64_t *seed);
uint64_t mix13(uint64_t key);
void write_u64(void *data, uint64_t x);
uint64_t read_u64(const void *data);
void write_u32(void *data, uint32_t x);
uint32_t read_u32(const void *data);
uint32_t crc32(const void *data, size_t len);
ssize_t read_full(int fd, void *data, size_t len);
uint32_t fnv1a_case(const char* buf, size_t len);

void binprint(const void *bin, size_t len);

#if INTPTR_MAX == INT64_MAX
#ifdef NO48BITPTRS
#define PTRSIZE 8
#else
#define PTRSIZE 6
#endif
#elif INTPTR_MAX == INT32_MAX
#define PTRSIZE 4
#else
#error Unknown pointer size
#endif

void *load_ptr(const uint8_t data[PTRSIZE]);
void store_ptr(uint8_t data[PTRSIZE], void *ptr);
uint8_t morris_incr(uint8_t morris, uint64_t rand);

#endif
