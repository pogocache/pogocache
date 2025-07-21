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
// Unit tls.c provides an interface for translating TLS bytes streams.
// This is intended to be used with client connections.
#include <errno.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "tls.h"
#include "xmalloc.h"
#include "openssl.h"

#ifdef NOOPENSSL

void tls_init(void) {}
bool tls_accept(int fd, struct tls **tls_out) {
    (void)fd;
    *tls_out = 0;
    return true;
}
int tls_close(struct tls *tls, int fd) {
    (void)tls;
    return close(fd);
}
ssize_t tls_read(struct tls *tls, int fd, void *data, size_t len) {
    (void)tls;
    return read(fd, data, len);
}
ssize_t tls_write(struct tls *tls, int fd, const void *data, size_t len) {
    (void)tls;
    return write(fd, data, len);
}
#else

extern const bool usetls;
extern const char *tlscertfile;
extern const char *tlscacertfile;
extern const char *tlskeyfile;

static SSL_CTX *ctx;

struct tls {
    SSL *ssl;
};

void tls_init(void) {
    if (!usetls) {
        return;
    }
    ctx = SSL_CTX_new(TLS_server_method());
    if (!SSL_CTX_load_verify_locations(ctx, tlscacertfile, 0)) {
        printf("# Error initializing tls, details to follow...\n");
        ERR_print_errors_fp(stderr);
        exit(1);
    }
    if (!SSL_CTX_use_certificate_file(ctx, tlscertfile , SSL_FILETYPE_PEM)) {
        printf("# Error initializing tls, details to follow...\n");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    if (!SSL_CTX_use_PrivateKey_file(ctx, tlskeyfile, SSL_FILETYPE_PEM)) {
        printf("# Error initializing tls, details to follow...\n");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    if (!SSL_CTX_check_private_key(ctx)) {
        printf("# tls: private key does not match the certificate\n");
        exit(EXIT_FAILURE);
    }
}

bool tls_accept(int fd, struct tls **tls_out) {
    if (!usetls) {
        // tls is disabled for all of pogocache.
        *tls_out = 0;
        return true;
    }
    SSL *ssl = SSL_new(ctx);
    if (!ssl) {
        printf("# tls: SSL_new() failed\n");
        *tls_out = 0;
        return false;
    }
    SSL_set_fd(ssl, fd);
    SSL_set_verify(ssl, SSL_VERIFY_PEER, 0);
    int ret = SSL_accept(ssl);
    if (ret <= 0) {
        int err = SSL_get_error(ssl, ret);
        if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
            printf("# tls: SSL_accept() failed\n");
            ERR_print_errors_fp(stderr);
            SSL_free(ssl);
            *tls_out = 0;
            return false;
        }
    }
    struct tls *tls = xmalloc(sizeof(struct tls));
    memset(tls, 0, sizeof(struct tls));
    tls->ssl = ssl;
    *tls_out = tls;
    return true;
}

int tls_close(struct tls *tls, int fd) {
    if (tls) {
        if (SSL_shutdown(tls->ssl) == 0) {
            SSL_shutdown(tls->ssl);
        }
        SSL_free(tls->ssl);
        xfree(tls);
    }
    return close(fd);
}

ssize_t tls_write(struct tls *tls, int fd, const void *data, size_t len) {
    if (!tls) {
        return write(fd, data, len);
    }
    size_t nbytes;
    int ret = SSL_write_ex(tls->ssl, data, len, &nbytes);
    if (ret == 1) {
        return nbytes;
    }
    int err = SSL_get_error(tls->ssl, ret);
    if (err == SSL_ERROR_ZERO_RETURN) {
        return 0;
    }
    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
        // Non-blocking I/O, try again later
        errno = EAGAIN;
    } else {
        // Unreliable errno. Fallback to EIO.
        errno = EIO;
    }
    return -1;
}

ssize_t tls_read(struct tls *tls, int fd, void *data, size_t len) {
    if (!tls) {
        return read(fd, data, len);
    }
    size_t nbytes;
    int ret = SSL_read_ex(tls->ssl, data, len, &nbytes);
    if (ret == 1) {
        return nbytes;
    }
    int err = SSL_get_error(tls->ssl, ret);
    if (err == SSL_ERROR_ZERO_RETURN) {
        return 0;
    }
    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
        // Non-blocking I/O, try again later
        errno = EAGAIN;
    } else { 
        // Unreliable errno. Fallback to EIO.
        errno = EIO;
    }
    return -1;
}

#endif
