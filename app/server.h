#ifndef SERVER_H
#define SERVER_H

#include <stdbool.h>

#include "storage.h"

extern const char* REPLICATION_ROLE_MASTER;
extern const char* REPLICATION_ROLE_SLAVE;

extern KeyValueStore* global_store;

typedef struct {
	int port;
	int fd;
} ConnectedSlave;

typedef struct {
	char* replid;
	int repl_offset;
	int num_connected_slaves;
	ConnectedSlave** connected_slaves;
} MasterRedisInfo;

typedef struct {
	char* master_host;
	int master_port;
} SlaveRedisInfo;

typedef struct {
	const char* role;
	int fd;
	int port;
	union {
		MasterRedisInfo* master;
		SlaveRedisInfo* slave;	
	} info;
} RedisServer;

extern RedisServer* global_server;

RedisServer* create_redis_server(const char*);
void free_redis_server(RedisServer*);

MasterRedisInfo* create_master_redis_info();
void free_master_redis_info(MasterRedisInfo*);

SlaveRedisInfo* create_slave_redis_info();
void free_slave_redis_info(SlaveRedisInfo*);

int connect_replica_to_master(RedisServer*);

bool ami_slave();
bool ami_master();

#endif /* SERVER_H */
