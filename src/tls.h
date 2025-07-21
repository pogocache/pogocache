// https://github.com/tidwall/pogocache
//
// Copyright 2025 Polypoint Labs, LLC. All rights reserved.
// This file is part of the Pogocache project.
// Use of this source code is governed by the AGPL that can be found in
// the LICENSE file.
//
// For alternative licensing options or general questions, please contact
// us at licensing@polypointlabs.com.
#ifndef TLS_H
#define TLS_H

struct tls;

void tls_init(void);
bool tls_accept(int fd, struct tls **tls);
int tls_close(struct tls *tls, int fd);
ssize_t tls_write(struct tls *tls, int fd, const void *data, size_t len);
ssize_t tls_read(struct tls *tls, int fd, void *data, size_t len);

#endif
