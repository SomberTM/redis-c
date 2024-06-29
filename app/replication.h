#ifndef REPLICATION_H
#define REPLICATION_H

#include "server.h"
#include "parser.h"

int connect_to_master(RedisServer*);
void send_handshake(int);
void propagate_command(RespArray*);

#endif /* REPLICATION_H */
