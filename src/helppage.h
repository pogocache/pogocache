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
// Header helppage.h is an information page for root requests
// through the HTTP wire protocol.
#ifndef HELPPAGE_H
#define HELPPAGE_H

#define HTTPLINK "https://github.com/tidwall/pogocache"

#define HELPPAGE_HTML                                      \
    "<html>\n"                                             \
    "<head>\n"                                             \
    "<title>Pogocache</title>\n"                           \
    "</head>\n"                                            \
    "<body>\n"                                             \
    "<h2>Welcome to the Pogocache HTTP interface.</h2>\n"  \
    "Visit <a href=\"" HTTPLINK "\">" HTTPLINK "</a> "     \
    "for documentation and examples.\n"                    \
    "</body>\n"                                            \
    "</html>\n"                                            \

#define HELPPAGE_TEXT                                      \
    "Welcome to the Pogocache HTTP interface.\n"           \
    "Visit " HTTPLINK " for documentation and examples.\n" \

#endif
