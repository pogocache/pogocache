// https://github.com/tidwall/pogocache
//
// Copyright 2025 Polypoint Labs, LLC. All rights reserved.
// This file is part of the Pogocache project.
// Use of this source code is governed by the MIT that can be found in
// the LICENSE file.
//
// For alternative licensing options or general questions, please contact
// us at licensing@polypointlabs.com.
#ifndef PARSE_H
#define PARSE_H

#include <stddef.h>
#include <stdbool.h>
#include "conn.h"
#include "args.h"
#include "buf.h"
#include "hashmap.h"
#include "postgres.h"

#define MAXARGS       100000      // Maximum number of arguments
#define MAXARGSZ      536870912   // Maximum size of a single argument (500MB)

const char *parse_lasterror(void);
size_t parse_lastmc_n(void);
ssize_t parse_command(const void *data, size_t len, struct args *args, 
    int *proto, bool *noreply, int *httpvers, bool *keepalive, struct pg **pg);

bool mc_valid_key(struct args *args, int i);

#define bytes_const_eq(data, len, str) \
    ((len) == sizeof(str)-1 && \
    memcmp((data), (str), sizeof(str)-1) == 0)

#define arg_const_eq(args, idx, str) \
    bytes_const_eq((args)->bufs[(idx)].data, (args)->bufs[(idx)].len, (str))

#define take_and_append_arg(at) \
    args_append(&args2, 0, 0, true); \
    args2.bufs[args2.len-1] = args->bufs[(at)]; \
    args->bufs[(at)] = (struct buf){ 0 }

extern __thread char parse_lasterr[1024];

#define parse_errorf(...) \
    snprintf(parse_lasterr, sizeof(parse_lasterr), __VA_ARGS__);

#define parse_seterror(msg) \
    parse_errorf("%s", (msg));


const char *parse_lasterror(void);

#endif
