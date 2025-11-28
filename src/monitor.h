// https://github.com/tidwall/pogocache
//
// Copyright 2025 Polypoint Labs, LLC. All rights reserved.
// This file is part of the Pogocache project.
// Use of this source code is governed by the MIT that can be found in
// the LICENSE file.
//
// For alternative licensing options or general questions, please contact
// us at licensing@polypointlabs.com.
#ifndef MONITOR_H
#define MONITOR_H

#include "conn.h"
#include "args.h"

void monitor_start(struct conn *conn);
void monitor_stop(struct conn *conn);
void monitor_cmd(int64_t now, int db, const char *addr, struct args *args);

#endif
