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
// Header openssl.h exposes an interface to openssl signatures.
#ifndef OPENSSL_H
#define OPENSSL_H

// This is an abbreviated header for the OpenSSL library.
// The types and method signatures are part of the OpenSSL 3.5 Final Release,
// as of Apr 8, 2025. 
// OpenSSL 3.5 is a long term stable (LTS) release. 
// Per OpenSSL’s LTS policy, 3.5 will be supported until April 8, 2030, and
// the ABI must remain stable until then.
// To expose the full headers use -DUSEFULLOPENSSLHEADER.

#if USEFULLOPENSSLHEADER

#include <openssl/ssl.h>
#include <openssl/err.h>

#else

#include <stdio.h>

#define X509_FILETYPE_PEM       1
#define X509_FILETYPE_ASN1      2
#define X509_FILETYPE_DEFAULT   3
#define SSL_FILETYPE_ASN1       X509_FILETYPE_ASN1
#define SSL_FILETYPE_PEM        X509_FILETYPE_PEM

#define SSL_VERIFY_NONE                 0x00
#define SSL_VERIFY_PEER                 0x01
#define SSL_VERIFY_FAIL_IF_NO_PEER_CERT 0x02
#define SSL_VERIFY_CLIENT_ONCE          0x04
#define SSL_VERIFY_POST_HANDSHAKE       0x08

#define SSL_ERROR_NONE                  0
#define SSL_ERROR_SSL                   1
#define SSL_ERROR_WANT_READ             2
#define SSL_ERROR_WANT_WRITE            3
#define SSL_ERROR_WANT_X509_LOOKUP      4
#define SSL_ERROR_SYSCALL               5
#define SSL_ERROR_ZERO_RETURN           6
#define SSL_ERROR_WANT_CONNECT          7
#define SSL_ERROR_WANT_ACCEPT           8
#define SSL_ERROR_WANT_ASYNC            9
#define SSL_ERROR_WANT_ASYNC_JOB       10
#define SSL_ERROR_WANT_CLIENT_HELLO_CB 11
#define SSL_ERROR_WANT_RETRY_VERIFY    12

typedef struct ssl_st SSL;
typedef struct ssl_ctx_st SSL_CTX;
typedef struct x509_store_ctx_st X509_STORE_CTX;
typedef struct ssl_method_st SSL_METHOD;

const SSL_METHOD *TLS_server_method(void);
const SSL_METHOD *TLS_client_method(void);
SSL_CTX *SSL_CTX_new(const SSL_METHOD *meth);
void ERR_print_errors_fp(FILE *fp);
int SSL_CTX_use_PrivateKey_file(SSL_CTX *ctx, const char *file, int type);
int SSL_get_error(const SSL *s, int ret_code);
int SSL_accept(SSL *ssl);
void SSL_free(SSL *ssl);
SSL *SSL_new(SSL_CTX *ctx);
void SSL_CTX_free(SSL_CTX *ctx);
int SSL_set_fd(SSL *s, int fd);
int SSL_connect(SSL *ssl);
typedef int (*SSL_verify_cb)(int preverify_ok, X509_STORE_CTX *x509_ctx);
void SSL_set_verify(SSL *s, int mode, SSL_verify_cb callback);
int SSL_shutdown(SSL *s);
int SSL_write_ex(SSL *s, const void *buf, size_t num, size_t *written);
int SSL_read_ex(SSL *ssl, void *buf, size_t num, size_t *readbytes);
int SSL_CTX_load_verify_locations(SSL_CTX *ctx, const char *CAfile,
    const char *CApath);
int SSL_CTX_use_PrivateKey_file(SSL_CTX *ctx, const char *file, int type);
int SSL_CTX_use_certificate_file(SSL_CTX *ctx, const char *file, int type);
int SSL_CTX_check_private_key(const SSL_CTX *ctx);
int SSL_write(SSL *ssl, const void *buf, int num);
int SSL_read(SSL *ssl, void *buf, int num);

#endif


#endif
