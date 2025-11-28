// https://github.com/tidwall/pogocache
//
// Copyright 2025 Polypoint Labs, LLC. All rights reserved.
// This file is part of the Pogocache project.
// Use of this source code is governed by the MIT that can be found in
// the LICENSE file.
//
// For alternative licensing options or general questions, please contact
// us at licensing@polypointlabs.com.
//
// Unit sys.c provides various system-level functions.
#if __linux__
#define _GNU_SOURCE
#endif
#include <stdatomic.h>
#include <unistd.h>
#include <sched.h>
#include <stdbool.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#if defined(__APPLE__)
#include <mach/mach_time.h>
#include <mach/mach.h>
#include <sys/utsname.h>
#elif defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <sys/utsname.h>
#elif defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

#include "sys.h"

int sys_nprocs(void) {
    static atomic_int nprocsa = 0;
    int nprocs = atomic_load_explicit(&nprocsa, __ATOMIC_RELAXED);
    if (nprocs > 0) {
        return nprocs;
    }
    int logical = sysconf(_SC_NPROCESSORS_CONF);
    logical = logical < 1 ? 1 : logical;
    int physical = logical;
    int affinity = physical;
#ifdef __linux__
    affinity = 0;
    cpu_set_t mask;
    CPU_ZERO(&mask);
    if (sched_getaffinity(0, sizeof(mask), &mask) == -1) {
        perror("sched_getaffinity");
        return 1;
    }
    for (int i = 0; i < CPU_SETSIZE; i++) {
        if (CPU_ISSET(i, &mask)) {
            affinity++;
        }
    }
    double hyper = ceil((double)logical / (double)physical);
    hyper = hyper < 1 ? 1 : hyper;
    affinity /= hyper;
#endif
    nprocs = affinity;
    nprocs = nprocs < 1 ? 1 : nprocs;
    atomic_store_explicit(&nprocsa, nprocs, __ATOMIC_RELAXED);
    return nprocs;
}

#if !defined(__linux__) && !defined(__EMSCRIPTEN__)
#include <sys/sysctl.h>
#endif

size_t sys_memory(void) {
    size_t sysmem = 0;
#ifdef __linux__
    FILE *f = fopen("/proc/meminfo", "rb");
    if (f) {
        char buf[4096];
        size_t n = fread(buf, 1, sizeof(buf)-1, f);
        buf[n] = '\0';
        char *s = 0;
        char *e = 0;
        s = strstr(buf, "MemTotal");
        if (s) s = strstr(s, ": ");
        if (s) e = strstr(s, "\n");
        if (e) {
            *e = '\0';
            s += 2;
            while (isspace(*s)) s++;
            if (strstr(s, " kB")) {
                s[strstr(s, " kB")-s] = '\0';
            }
            errno = 0;
            char *end;
            int64_t isysmem = strtoll(s, &end, 10);
            assert(errno == 0 && isysmem > 0);
            isysmem *= 1024;
            sysmem = isysmem;
        }
        fclose(f);
    }
#elif defined(__EMSCRIPTEN__)
    size_t heap_size = EM_ASM_INT({
        return HEAP8.buffer.byteLength;
    });
    return heap_size;
#else
    size_t memsize = 0;
    size_t len = sizeof(memsize);
    if (sysctlbyname("hw.memsize", &memsize, &len, 0, 0) == 0) {
        sysmem = memsize;
    }
#endif
    return sysmem;
}

uint64_t sys_seed(void) {
#ifdef __EMSCRIPTEN__
    return 0;
#else
    #define NSEEDCAP 64
    static __thread int nseeds = 0;
    static __thread uint64_t seeds[NSEEDCAP];
    if (nseeds == 0) {
        // Generate a group of new seeds
        FILE *f = fopen("/dev/urandom", "rb");
        if (!f) {
            perror("# /dev/urandom");
            exit(1);
        }
        size_t n = fread(seeds, 8, NSEEDCAP, f);
        (void)n;
        assert(n == NSEEDCAP);
        fclose(f);
        nseeds = NSEEDCAP;
    }
    return seeds[--nseeds];
#endif
}

#ifdef __EMSCRIPTEN__
static int64_t add_time_delta = 0;
void add_time(int extra_milliseconds) {
    add_time_delta += (int64_t)extra_milliseconds * (int64_t)1000000;
}
#endif

static int64_t nanotime(struct timespec *ts) {
    int64_t x = ts->tv_sec;
    x *= 1000000000;
    x += ts->tv_nsec;
#ifdef __EMSCRIPTEN__
    x += add_time_delta;
#endif
    return x;
}

// Return monotonic nanoseconds of the CPU clock.
int64_t sys_now(void) {
    struct timespec now = { 0 };
#ifdef __linux__
    clock_gettime(CLOCK_BOOTTIME, &now);
#elif defined(__APPLE__)
    clock_gettime(CLOCK_UPTIME_RAW, &now);
#else
    clock_gettime(CLOCK_MONOTONIC, &now);
#endif
    return nanotime(&now);
}

// Return unix timestamp in nanoseconds
int64_t sys_unixnow(void) {
    struct timespec now = { 0 };
    clock_gettime(CLOCK_REALTIME, &now);
    return nanotime(&now);
}

#ifdef __APPLE__
void sys_getmeminfo(struct sys_meminfo *info) {
    task_basic_info_data_t taskInfo;
    mach_msg_type_number_t infoCount = TASK_BASIC_INFO_COUNT;
    kern_return_t kr = task_info(mach_task_self(), TASK_BASIC_INFO,
        (task_info_t)&taskInfo, &infoCount);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "# task_info: %s\n", mach_error_string(kr));
        abort();
    }
    info->virt = taskInfo.virtual_size;
    info->rss = taskInfo.resident_size;
}
#elif defined(__EMSCRIPTEN__) 
void sys_getmeminfo(struct sys_meminfo *info) {
    info->virt = 0;
    info->rss = 0;
}
#elif __linux__
void sys_getmeminfo(struct sys_meminfo *info) {
    FILE *f = fopen("/proc/self/statm", "r");
    if (!f) {
        perror("# open /proc/self/statm");
        abort();
    }
    unsigned long vm_pages, rss_pages;
    long x = fscanf(f, "%lu %lu", &vm_pages, &rss_pages);
    fclose(f);
    if (x != 2) {
        perror("# read /proc/self/statm");
        abort();
    }

    // Get the system page size (in bytes)
    size_t page_size = sysconf(_SC_PAGESIZE);
    assert(page_size > 0);

    // Convert pages to bytes
    info->virt = vm_pages * page_size;
    info->rss = rss_pages * page_size;
}
#endif

#include <sys/utsname.h>

const char *sys_arch(void) {
    static __thread bool got = false;
    static __thread char arch[1024] = "unknown/unknown";
    if (!got) {
        struct utsname uts;
        if (uname(&uts) == 0) {
            snprintf(arch, sizeof(arch), "%s/%s", uts.sysname,
                strcmp(uts.machine, "aarch64")==0?"arm64":
                strcmp(uts.machine, "x86_64")==0?"amd64":
                uts.machine);
            char *p = arch;
            while (*p) {
                *p = tolower(*p);
                p++;
            }
            got = true;
        }
    }
    return arch;
}

const char *sys_libc(void) {
#ifdef __APPLE__
    return "libSystem";
#elif defined(__MUSL__)
    return "musl";
#elif defined(__EMSCRIPTEN__)
    return "emcc";
#elif defined(__GLIBC__)
    return "glibc";
#else
    return "unknown";
#endif
}

void sys_genuseid(char useid[16]) {
    const uint8_t chs[] = 
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789";
    uint64_t a = sys_seed();
    uint64_t b = sys_seed();
    uint8_t bytes[16];
    memcpy(bytes, &a, 8);
    memcpy(bytes+8, &b, 8);
    for (int i = 0; i < 16; i++) {
        bytes[i] = chs[bytes[i]%62];
    }
    memcpy(useid, bytes, 16);
}

// Returns a unique thread id for the current thread.
// This is an artificial generated value that is always distinct. 
uint64_t sys_threadid(void) {
    static atomic_int_fast64_t next = 0;
    static __thread uint64_t id = 0;
    if (id == 0) {
        id = atomic_fetch_add_explicit(&next, 1, __ATOMIC_RELEASE);
    }
    return id;
}

const char *sys_os(void) {
    static __thread char buf[1024];
#ifdef __linux__
    char fdata[512];
    FILE *f = fopen("/etc/os-release", "r");
    if (f) {
        size_t n = fread(fdata, 1, sizeof(fdata)-1, f);
        fclose(f);
        fdata[n] = '\0';
        char *name = strstr(fdata, "PRETTY_NAME=\"");
        if (name) {
            name += 13;
            char *end = strstr(name, "\"");
            if (end) {
                *end = '\0';
                snprintf(buf, sizeof(buf), "%s", name);
                return buf;
            }
        }
    }
#elif defined(__APPLE__)
    char fdata[512];
    FILE *f = popen("sw_vers -productVersion", "r");
    if (f) {
        size_t n = fread(fdata, 1, sizeof(fdata)-1, f);
        pclose(f);
        fdata[n] = '\0';
        if (n > 0 && fdata[n-1] == '\n') {
            fdata[n-1] = '\0';
            snprintf(buf, sizeof(buf), "macOS %s", fdata);
            return buf;
        }
    }
#endif
    struct utsname uts;
    if (uname(&uts) == 0) {
        snprintf(buf, sizeof(buf), "%s %s", uts.sysname, uts.release);
    } else {
        snprintf(buf, sizeof(buf), "unknown");
    }
    return buf;
}
