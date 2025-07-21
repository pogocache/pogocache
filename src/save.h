// https://github.com/tidwall/pogocache
//
// Copyright 2025 Polypoint Labs, LLC. All rights reserved.
// This file is part of the Pogocache project.
// Use of this source code is governed by the AGPL that can be found in
// the LICENSE file.
//
// For alternative licensing options or general questions, please contact
// us at licensing@polypointlabs.com.
#ifndef SAVE_H
#define SAVE_H

#include <stdbool.h>

struct load_stats {
    size_t ninserted; // total number of inserted entries
    size_t nexpired;  // total number of expires entries 
    size_t csize;     // compressed size
    size_t dsize;     // decompressed size
};

int save(const char *path, bool fast);
int load(const char *path, bool fast, struct load_stats *stats);
bool cleanwork(const char *path);

#endif
