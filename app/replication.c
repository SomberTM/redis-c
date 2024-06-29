#include <assert.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "replication.h"
#include "encoder.h"
#include "utils.h"

int connect_to_master(RedisServer* server) { 
	assert(server->role == REPLICATION_ROLE_SLAVE);

	SlaveRedisInfo* info = server->info.slave;
	printf("Replica attempting to connect to master at %s:%d\n", info->master_host, info->master_port);

	int master_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (master_fd == -1) {
		printf("Socket creation failed: %s...\n", strerror(errno));
		return -1;
	}

	struct sockaddr_in serv_addr;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(info->master_port);

	char* host = strcmp(info->master_host, "localhost") == 0 ? "127.0.0.1" : info->master_host;
	int bin = inet_pton(AF_INET, host, &serv_addr.sin_addr);
	if (bin <= 0) {
		printf("Invalid address family: %s...\n", strerror(errno));
		return -1;
	}

	int status = connect(master_fd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	if (status == -1) {
		printf("Replica to master connection failed: %s\n", strerror(errno));
		return -1;
	}

	printf("Replica connected to master at %s:%d\n", info->master_host, info->master_port);
	return master_fd;

}

void send_handshake(int master_fd) {
	char trash[1024];
	size_t n;

	send(master_fd, PING_REQUEST, strlen(PING_REQUEST), 0);
	n = read(master_fd, trash, 1024);
	trash[n] = '\0';
	printf("REPLICA HANDSHAKE (1/4) PING RESPONSE: ");
	raw_print(trash);
	printf("\n");

	char replconf_port[256];
	sprintf(replconf_port, "*3\r\n$8\r\nREPLCONF\r\n$14\r\nlistening-port\r\n$4\r\n%d\r\n", global_server->port);
	send(master_fd, replconf_port, strlen(replconf_port), 0);
	n = read(master_fd, trash, 1024);
	trash[n] = '\0';
	printf("REPLICA HANDSHAKE (2/4) REPLCONF LISTENING-PORT RESPONSE: ");
	raw_print(trash);
	printf("\n");

	char* replconf_capa = "*3\r\n$8\r\nREPLCONF\r\n$4\r\ncapa\r\n$6\r\npsync2\r\n";
	send(master_fd, replconf_capa, strlen(replconf_capa), 0);
	n = read(master_fd, trash, 1024);
	trash[n] = '\0';
	printf("REPLICA HANDSHAKE (3/4) REPLCONF CAPA RESPONSE: ");
	raw_print(trash);
	printf("\n");

	char* psync = "*3\r\n$5\r\nPSYNC\r\n$1\r\n?\r\n$2\r\n-1\r\n";
	send(master_fd, psync, strlen(psync), 0);
	n = read(master_fd, trash, 1024);
	trash[n] = '\0';
	printf("REPLICA HANDSHAKE (4/4) REPLCONF PSYNC RESPONSE: ");
	raw_print(trash);
	printf("\n");
}

void propagate_command(RespArray* array) {
}
