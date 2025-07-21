// https://github.com/tidwall/pogocache
//
// Copyright 2025 Polypoint Labs, LLC. All rights reserved.
// This file is part of the Pogocache project.
// Use of this source code is governed by the AGPL that can be found in
// the LICENSE file.
//
// For alternative licensing options or general questions, please contact
// us at licensing@polypointlabs.com.
#ifndef SYS_H
#define SYS_H

#include <stddef.h>
#include <stdint.h>

int sys_nprocs(void);
size_t sys_memory(void);
uint64_t sys_seed(void);
int64_t sys_now(void);
int64_t sys_unixnow(void);
const char *sys_arch(void);
void sys_genuseid(char useid[16]);

struct sys_meminfo {
    size_t virt;
    size_t rss;
};

void sys_getmeminfo(struct sys_meminfo *info); 
uint64_t sys_threadid(void);

#endif
