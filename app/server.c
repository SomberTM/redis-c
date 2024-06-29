#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <ctype.h>
#include <assert.h>

#include "parser.h"
#include "storage.h"
#include "encoder.h"
#include "utils.h"
#include "server.h"
#include "commands.h"
#include "replication.h"

const char* REPLICATION_ROLE_MASTER = "master";
const char* REPLICATION_ROLE_SLAVE = "slave";

#define MAX_CONCURRENT_CLIENTS 10

pthread_t thread_pool[MAX_CONCURRENT_CLIENTS];

KeyValueStore* global_store;
RedisServer* global_server;
const char* replication_role;

pthread_mutex_t accept_mutex;

void* accept_connection(void*);

void sigint_handler(int sig) {
	for (int i = 0; i < MAX_CONCURRENT_CLIENTS; i++)
		pthread_cancel(thread_pool[i]);

	close(global_server->fd);

	free_kv_store(global_store);
	global_store = NULL;

	free_redis_server(global_server);
	global_server = NULL;

	exit(0);
}

bool ami_slave() {
	if (global_server == NULL) return false;
	return global_server->role == REPLICATION_ROLE_SLAVE;
}

bool ami_master() {
	if (global_server == NULL) return false;
	return global_server->role == REPLICATION_ROLE_MASTER;
}

RedisServer* create_redis_server(const char* role) {
	RedisServer* server = malloc(sizeof(RedisServer));
	server->role = role;
	server->fd = -1;
	server->port = 6379;
	
	if (role == REPLICATION_ROLE_MASTER)
		server->info.master = create_master_redis_info();
	else if (role == REPLICATION_ROLE_SLAVE)
		server->info.slave = create_slave_redis_info();

	return server;
}

void free_redis_server(RedisServer* server) {
	if (server->role == REPLICATION_ROLE_MASTER)
		free(server->info.master);
	else if (server->role == REPLICATION_ROLE_SLAVE)
		free(server->info.slave);

	server->info.master = NULL;
	server->info.slave = NULL;

	free(server);
	server = NULL;
}

MasterRedisInfo* create_master_redis_info() {
	MasterRedisInfo* info = malloc(sizeof(MasterRedisInfo));
	info->replid = "8371b4fb1155b71f4a04d3e1bc3e18c4a990aeeb";
	info->repl_offset = 0;
	info->num_connected_slaves = 0;
	info->connected_slaves = NULL;
	return info;
}

SlaveRedisInfo* create_slave_redis_info() {
	SlaveRedisInfo* info = malloc(sizeof(SlaveRedisInfo));
	return info;
}

int main(int argc, char* argv[]) {
	// Disable output buffering
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	printf("Program Arguments (%d) [", argc);
	for (int i = 0; i < argc; i++) {
		printf("%s", argv[i]);
		if (i < argc - 1)
			printf(", ");
	}
	printf("]\n");

	int port = 6379;
	if (argc >= 3) {
		for (int i = 0; i < argc; i++) {
			char* arg = argv[i];
			if (strcmp(arg, "--port") == 0)
				port = atoi(argv[i + 1]);
			else if (strcmp(arg, "--replicaof") == 0) {
				global_server = create_redis_server(REPLICATION_ROLE_SLAVE);

				char* info = argv[i + 1];
				char* host = strtok(info, " ");
				char* port = strtok(NULL, " ");

				global_server->info.slave->master_host = host;
				global_server->info.slave->master_port = atoi(port);
			}
		}
	}

	if (global_server == NULL)
		global_server = create_redis_server(REPLICATION_ROLE_MASTER);

	global_server->port = port;

	// You can use print statements as follows for debugging, they'll be visible when running tests.
	printf("Logs from your program will appear here!\n");

	// Uncomment this block to pass the first stage
	int server_fd;
	
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == -1) {
		printf("Socket creation failed: %s...\n", strerror(errno));
		return 1;
	}
	global_server->fd = server_fd;

	// Since the tester restarts your program quite often, setting SO_REUSEADDR
	// ensures that we don't run into 'Address already in use' errors
	int reuse = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
		printf("SO_REUSEADDR failed: %s \n", strerror(errno));
		return 1;
	}
	
	struct sockaddr_in serv_addr = { .sin_family = AF_INET ,
					 .sin_port = htons(global_server->port),
					 .sin_addr = { htonl(INADDR_ANY) },
					};
	
	if (bind(server_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
		printf("Bind failed: %s \n", strerror(errno));
		return 1;
	}
	
	int connection_backlog = 5;
	if (listen(server_fd, connection_backlog) != 0) {
		printf("Listen failed: %s \n", strerror(errno));
		return 1;
	}

	printf("Listening on port %d\n", global_server->port);

	global_store = create_kv_store();
	pthread_mutex_init(&accept_mutex, NULL);

	if (ami_slave()) {
		char trash[1024];

		int master_fd = connect_to_master(global_server);
		send_handshake(master_fd);

		close(master_fd);

		// Replica will have one client, the master server
		pthread_t thread;
		pthread_attr_t thread_attr;

		pthread_attr_init(&thread_attr);
		pthread_attr_setscope(&thread_attr, PTHREAD_SCOPE_SYSTEM);

		thread_pool[0] = thread;
		pthread_create(&thread, &thread_attr, accept_connection, &server_fd);

		pthread_attr_destroy(&thread_attr);
		pthread_join(thread, NULL);
	} else if (ami_master()) {
		for (int i = 0; i < MAX_CONCURRENT_CLIENTS; i++) {
			pthread_attr_t thread_attr;

			pthread_attr_init(&thread_attr);
			pthread_attr_setscope(&thread_attr, PTHREAD_SCOPE_SYSTEM);

			pthread_create(&thread_pool[i], &thread_attr, accept_connection, &server_fd);

			pthread_attr_destroy(&thread_attr);
		}

		for (int i = 0; i < MAX_CONCURRENT_CLIENTS; i++) {
			pthread_join(thread_pool[i], NULL);
		}
	}

	free_kv_store(global_store);
	global_store = NULL;

	free_redis_server(global_server);
	global_server = NULL;

	close(server_fd);

	return 0;
}

void* accept_connection(void* server_fd_ptr) {
	int server_fd = *(int*)(server_fd_ptr);

	struct sockaddr_in client_addr;
	int client_addr_len = sizeof(client_addr);

	while (1) {
		printf("Waiting for a client to connect...\n");

		pthread_mutex_lock(&accept_mutex);
		int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);
		pthread_mutex_unlock(&accept_mutex);

		printf("Client connected\n");

		char buffer[1024];
		size_t n;
		while ((n = read(client_fd, buffer, 1024)) > 0) {
			buffer[n] = '\0';

			printf("Received bytes: ");
			raw_print(buffer);
			printf("\n");

			RespParser* parser = create_resp_parser(buffer);

			printf("Raw (%d) [", parser->items_length);
			for (size_t i = 0; i < parser->items_length; i++) {
				printf("%s", parser->items[i]);
			if (i < parser->items_length - 1)
				printf(", ");
			}
			printf("]\n");

			size_t num_datas = 0;
			RespData** datas = execute_resp_parser(parser, &num_datas);
			parser = NULL;

			bool sent = false;

			if (num_datas == 0) {
				send(client_fd, "-Bad Request\r\n", 14, 0);
				sent = true;
			} else {
				print_resp_data_array(datas, num_datas);

				RespData* command_array = datas[0];
				if (command_array->type == RESP_ARRAY) {
					RespArray* array = command_array->value->array;

					const char* response = execute_command(array);
					if (response != NULL) {
						printf("Responding with: ");
						raw_print(response);
						printf("\n");

						send(client_fd, response, strlen(response), 0);
						sent = true;
					}
				}
			}

			if (!sent) {
				printf("No message was sent\n");
				send(client_fd, "-Bad Request\r\n", 14, 0);
			}

			free_resp_data_array(datas, num_datas);
			datas = NULL;

			print_kv_store(global_store);
		}

		close(client_fd);
	}

	return NULL;
}
