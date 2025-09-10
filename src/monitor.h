#ifndef MONITOR_H
#define MONITOR_H

#include "conn.h"
#include "args.h"

void monitor_start(struct conn *conn);
void monitor_stop(struct conn *conn);
void monitor_cmd(int64_t now, int db, const char *addr, struct args *args);

#endif
