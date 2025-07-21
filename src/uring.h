// https://github.com/tidwall/pogocache
//
// Copyright 2025 Polypoint Labs, LLC. All rights reserved.
// This file is part of the Pogocache project.
// Use of this source code is governed by the AGPL that can be found in
// the LICENSE file.
//
// For alternative licensing options or general questions, please contact
// us at licensing@polypointlabs.com.
#ifndef URING_H
#define URING_H

#ifndef NOURING
#ifdef __linux__
#include "../deps/liburing/src/include/liburing.h"
#else
#define NOURING
#endif
#endif

#include <stdbool.h>

bool uring_available(void);

#endif
