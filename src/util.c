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
// Unit util.c provides various utilities and convenience functions.
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "util.h"

// Performs a case-insenstive equality test between the byte slice 'data' and
// a c-string. It's expected that c-string is already lowercase and 
// null-terminated. The data does not need to be null-terminated.
bool argeq_bytes(const void *data, size_t datalen, const char *cstr) {
    const char *p = data;
    const char *e = p+datalen;
    bool eq = true;
    while (eq && p < e && *cstr) {
        eq = tolower(*p) == *cstr;
        p++;
        cstr++;
    }
    return eq && *cstr == '\0' && p == e;
}

bool argeq(struct args *args, int idx, const char *cstr) {
    return argeq_bytes(args->bufs[idx].data, args->bufs[idx].len, cstr);
}

// Safely adds two int64_t values and with clamping on overflow.
int64_t int64_add_clamp(int64_t a, int64_t b) {
    if (!((a ^ b) < 0)) { // Opposite signs can't overflow
        if (a > 0) {
            if (b > INT64_MAX - a) {
                return INT64_MAX;
            }
        } else if (b < INT64_MIN - a) {
            return INT64_MIN;
        }
    }
    return a + b;
}

// Safely multiplies two int64_t values and with clamping on overflow.
int64_t int64_mul_clamp(int64_t a, int64_t b) {
    if (a || b) {
        if (a > 0) {
            if (b > 0 && a > INT64_MAX / b) {
                return INT64_MAX;
            } else if (b < 0 && b < INT64_MIN / a) {
                return INT64_MIN;
            }
        } else {
            if (b > 0 && a < INT64_MIN / b) {
                return INT64_MIN;
            } else if (b < 0 && a < INT64_MAX / b) {
                return INT64_MAX;
            }
        }
    }
    return a * b;
}

/// https://github.com/tidwall/varint.c
int varint_write_u64(void *data, uint64_t x) {
    uint8_t *bytes = data;
    if (x < 128) {
        *bytes = x;
        return 1;
    }
    int n = 0;
    do {
        bytes[n++] = (uint8_t)x | 128;
        x >>= 7;
    } while (x >= 128);
    bytes[n++] = (uint8_t)x;
    return n;
}

int varint_read_u64(const void *data, size_t len, uint64_t *x) {
    const uint8_t *bytes = data;
    if (len > 0 && bytes[0] < 128) {
        *x = bytes[0];
        return 1;
    }
    uint64_t b;
    *x = 0;
    size_t i = 0;
    while (i < len && i < 10) {
        b = bytes[i]; 
        *x |= (b & 127) << (7 * i); 
        if (b < 128) {
            return i + 1;
        }
        i++;
    }
    return i == 10 ? -1 : 0;
}

int varint_write_i64(void *data, int64_t x) {
    uint64_t ux = (uint64_t)x << 1;
    ux = x < 0 ? ~ux : ux;
    return varint_write_u64(data, ux);
}

int varint_read_i64(const void *data, size_t len, int64_t *x) {
    uint64_t ux;
    int n = varint_read_u64(data, len, &ux);
    *x = (int64_t)(ux >> 1);
    *x = ux&1 ? ~*x : *x;
    return n;
}


const char *memstr(double size, char buf[64]) {
    if (size < 1024.0) {
        snprintf(buf, 64, "%0.0fB", size);
    } else if (size < 1024.0*1024.0) {
        snprintf(buf, 64, "%0.1fK", size/1024.0);
    } else if (size < 1024.0*1024.0*1024.0) {
        snprintf(buf, 64, "%0.1fM", size/1024.0/1024.0);
    } else {
        snprintf(buf, 64, "%0.1fG", size/1024.0/1024.0/1024.0);
    }
    char *dot;
    if ((dot=strstr(buf, ".0G"))) {
        memmove(dot, dot+2, 7);
    } else if ((dot=strstr(buf, ".0M"))) {
        memmove(dot, dot+2, 7);
    } else if ((dot=strstr(buf, ".0K"))) {
        memmove(dot, dot+2, 7);
    }
    return buf;
}

const char *memstr_long(double size, char buf[64]) {
    if (size < 1024.0) {
        snprintf(buf, 64, "%0.0f bytes", size);
    } else if (size < 1024.0*1024.0) {
        snprintf(buf, 64, "%0.1f KB", size/1024.0);
    } else if (size < 1024.0*1024.0*1024.0) {
        snprintf(buf, 64, "%0.1f MB", size/1024.0/1024.0);
    } else {
        snprintf(buf, 64, "%0.1f GB", size/1024.0/1024.0/1024.0);
    }
    char *dot;
    if ((dot=strstr(buf, ".0 GB"))) {
        memmove(dot, dot+2, 7);
    } else if ((dot=strstr(buf, ".0 MB"))) {
        memmove(dot, dot+2, 7);
    } else if ((dot=strstr(buf, ".0 KB"))) {
        memmove(dot, dot+2, 7);
    }
    return buf;
}

// https://zimbry.blogspot.com/2011/09/better-bit-mixing-improving-on.html
uint64_t mix13(uint64_t key) {
    key ^= (key >> 30);
    key *= UINT64_C(0xbf58476d1ce4e5b9);
    key ^= (key >> 27);
    key *= UINT64_C(0x94d049bb133111eb);
    key ^= (key >> 31);
    return key;
}

uint64_t rand_next(uint64_t *seed) {
    // pcg + mix13
    *seed = (*seed * UINT64_C(6364136223846793005)) + 1;
    return mix13(*seed);
}

void write_u64(void *data, uint64_t x) {
    uint8_t *bytes = data;
    bytes[0] = (x>>0)&0xFF;
    bytes[1] = (x>>8)&0xFF;
    bytes[2] = (x>>16)&0xFF;
    bytes[3] = (x>>24)&0xFF;
    bytes[4] = (x>>32)&0xFF;
    bytes[5] = (x>>40)&0xFF;
    bytes[6] = (x>>48)&0xFF;
    bytes[7] = (x>>56)&0xFF;
}

uint64_t read_u64(const void *data) {
    const uint8_t *bytes = data;
    uint64_t x = 0;
    x |= ((uint64_t)bytes[0])<<0;
    x |= ((uint64_t)bytes[1])<<8;
    x |= ((uint64_t)bytes[2])<<16;
    x |= ((uint64_t)bytes[3])<<24;
    x |= ((uint64_t)bytes[4])<<32;
    x |= ((uint64_t)bytes[5])<<40;
    x |= ((uint64_t)bytes[6])<<48;
    x |= ((uint64_t)bytes[7])<<56;
    return x;
}

void write_u32(void *data, uint32_t x) {
    uint8_t *bytes = data;
    bytes[0] = (x>>0)&0xFF;
    bytes[1] = (x>>8)&0xFF;
    bytes[2] = (x>>16)&0xFF;
    bytes[3] = (x>>24)&0xFF;
}

uint32_t read_u32(const void *data) {
    const uint8_t *bytes = data;
    uint32_t x = 0;
    x |= ((uint32_t)bytes[0])<<0;
    x |= ((uint32_t)bytes[1])<<8;
    x |= ((uint32_t)bytes[2])<<16;
    x |= ((uint32_t)bytes[3])<<24;
    return x;
}

// https://www.w3.org/TR/2003/REC-PNG-20031110/#D-CRCAppendix
uint32_t crc32(const void *data, size_t len) {
    static __thread uint32_t table[256];
    static __thread bool computed = false;
    if (!computed) {
        for (uint32_t n = 0; n < 256; n++) {
            uint32_t c = n;
            for (int k = 0; k < 8; k++) {
                c = (c&1)?0xedb88320L^(c>>1):c>>1;
            }
            table[n] = c;
        }
        computed = true;
    }
    uint32_t crc = ~0;
    const uint8_t *buf = data;
    for (size_t n = 0; n < len; n++) {
        crc = table[(crc^buf[n])&0xff]^(crc>>8);
    }
    return ~crc;
}

// Attempts to read exactly len bytes from file stream
// Returns the number of bytes read. Anything less than len means the stream
// was closed or an error occured while reading.
// Return -1 if no bytes were read and there was an error.
ssize_t read_full(int fd, void *data, size_t len) {
    uint8_t *bytes = data;
    size_t total = 0;
    while (len > 0) {
        ssize_t n = read(fd, bytes+total, len);
        if (n <= 0) {
            if (total > 0) {
                break;
            }
            return n;
        }
        len -= n;
        total += n;
    }
    return total;
}

size_t u64toa(uint64_t x, uint8_t *data) {
    if (x < 10) {
        data[0] = '0'+x;
        return 1;
    }
    size_t i = 0;
    do {
        data[i++] = '0' + x % 10;
    } while ((x /= 10) > 0);
    // reverse the characters
    for (size_t j = 0, k = i-1; j < k; j++, k--) {
        uint8_t ch = data[j];
        data[j] = data[k];
        data[k] = ch;
    }
    return i;
}

size_t i64toa(int64_t x, uint8_t *data) {
    if (x < 0) {
        data[0] = '-';
        data++;
        return u64toa(x * -1, data) + 1;
    }
    return u64toa(x, data);
}

uint32_t fnv1a_case(const char* buf, size_t len) {
    uint32_t hash = 0x811c9dc5;
    for (size_t i = 0; i < len; i++) {
        hash = (hash ^ tolower(buf[i])) * 0x01000193;
    }
	return hash;
}

bool parse_i64(const char *data, size_t len, int64_t *x) {
    char buf[24];
    if (len > 21) {
        return false;
    }
    memcpy(buf, data, len);
    buf[len] = '\0';
    errno = 0;
    char *end;
    *x = strtoll(buf, &end, 10);
    return errno == 0 && end == buf+len;
}

bool parse_u64(const char *data, size_t len, uint64_t *x) {
    char buf[24];
    if (len > 21) {
        return false;
    }
    memcpy(buf, data, len);
    buf[len] = '\0';
    if (buf[0] == '-') {
        return false;
    }
    errno = 0;
    char *end;
    *x = strtoull(buf, &end, 10);
    return errno == 0 && end == buf+len;
}

bool argi64(struct args *args, int idx, int64_t *x) {
    return parse_i64(args->bufs[idx].data, args->bufs[idx].len, x);
}

bool argu64(struct args *args, int idx, uint64_t *x) {
    return parse_u64(args->bufs[idx].data, args->bufs[idx].len, x);
}

void *load_ptr(const uint8_t data[PTRSIZE]) {
#if PTRSIZE == 4
    uint32_t uptr;
    memcpy(&uptr, data, 4);
    return (void*)(uintptr_t)uptr;
#elif PTRSIZE == 6
    uint64_t uptr = 0;
    uptr |= ((uint64_t)data[0])<<0;
    uptr |= ((uint64_t)data[1])<<8;
    uptr |= ((uint64_t)data[2])<<16;
    uptr |= ((uint64_t)data[3])<<24;
    uptr |= ((uint64_t)data[4])<<32;
    uptr |= ((uint64_t)data[5])<<40;
    return (void*)(uintptr_t)uptr;
#elif PTRSIZE == 8
    uint64_t uptr;
    memcpy(&uptr, data, 8);
    return (void*)(uintptr_t)uptr;
#endif
}

void store_ptr(uint8_t data[PTRSIZE], void *ptr) {
#if PTRSIZE == 4
    uint32_t uptr = (uintptr_t)(void*)ptr;
    memcpy(data, &uptr, 4);
#elif PTRSIZE == 6
    uint64_t uptr = (uintptr_t)(void*)ptr;
    data[0] = (uptr>>0)&0xFF;
    data[1] = (uptr>>8)&0xFF;
    data[2] = (uptr>>16)&0xFF;
    data[3] = (uptr>>24)&0xFF;
    data[4] = (uptr>>32)&0xFF;
    data[5] = (uptr>>40)&0xFF;
#elif PTRSIZE == 8
    uint64_t uptr = (uintptr_t)(void*)ptr;
    memcpy(data, &uptr, 8);
#endif
}

// Increment a morris counter. The counter is clipped to 31 bits
uint8_t morris_incr(uint8_t morris, uint64_t rand) {
    return morris>=31?31:morris+!(rand&((UINT64_C(1)<<morris)-1));
}

void binprint(const void *bin, size_t len) {
    for (size_t j = 0; j < len; j++) {
        uint8_t c = ((uint8_t*)bin)[j];
        if (c < ' ' || c > '~') {
            printf("\\x%02x", c);
        } else {
            printf("%c", c);
        }
    }
}
