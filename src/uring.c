// https://github.com/tidwall/pogocache
//
// Copyright 2025 Polypoint Labs, LLC. All rights reserved.
// This file is part of the Pogocache project.
// Use of this source code is governed by the AGPL that can be found in
// the LICENSE file.
//
// For alternative licensing options or general questions, please contact
// us at licensing@polypointlabs.com.
#include <stdatomic.h>
#include "uring.h"

bool uring_available(void) {
#ifdef NOURING
    return false;
#else
    static atomic_int available = -1;
    if (atomic_load_explicit(&available, __ATOMIC_ACQUIRE )== -1) {
        struct io_uring ring;
        if (io_uring_queue_init(1, &ring, 0) == 0) {
            io_uring_queue_exit(&ring);
            atomic_store(&available, 1);
        } else {
            atomic_store(&available, 0);
        }
    }
    return atomic_load_explicit(&available, __ATOMIC_ACQUIRE) == 1;
#endif
}
