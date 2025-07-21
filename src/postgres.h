// https://github.com/tidwall/pogocache
//
// Copyright 2025 Polypoint Labs, LLC. All rights reserved.
// This file is part of the Pogocache project.
// Use of this source code is governed by the AGPL that can be found in
// the LICENSE file.
//
// For alternative licensing options or general questions, please contact
// us at licensing@polypointlabs.com.
#ifndef POSTGRES_H
#define POSTGRES_H

#include "buf.h"
#include "args.h"

#define PGNAMEDATALEN 64

struct pg_statement {
    char name[PGNAMEDATALEN];
    struct args args;
    struct buf argtypes;
    int nparams;
};

struct pg_portal {
    char name[PGNAMEDATALEN];
    char stmt[PGNAMEDATALEN];
    struct args params;
};

struct pg {
    bool ssl;      // ssl request message received
    bool startup;  // startup message received
    bool auth;     // user is authenticated
    bool ready;    // ready for queries
    bool error;    // there's an error waiting
    bool describe; // describe message received
    bool parse;
    bool bind;
    bool execute;
    bool close;
    bool sync;
    bool empty_query;

    char *desc;   // describe response
    int desclen;  // describe response len

    int oid;      // current output oid (text or bytea)

    struct hashmap *statements; // prepared statements (struct pg_statment)
    struct hashmap *portals;    // bind portals        (struct pg_portal)

    struct args targs; // temporary args
    // struct args xargs; // execution args
    
    char *user;
    char *database;
    char *application_name;
    struct buf buf; // query read buffer
};

struct pg *pg_new(void);
void pg_free(struct pg *pg);
bool pg_respond(struct conn *conn, struct pg *pg);
void pg_free(struct pg *pg);

bool pg_precommand(struct conn *conn, struct args *args, struct pg *pg);

#endif

