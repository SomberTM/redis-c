#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <ctype.h>

#include "parser.h"

#define MAX_CONCURRENT_CLIENTS 5

pthread_mutex_t accept_mutex;

void* accept_connection(void*);

int main() {
	// Disable output buffering
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);
	
	// You can use print statements as follows for debugging, they'll be visible when running tests.
	printf("Logs from your program will appear here!\n");

	// Uncomment this block to pass the first stage
	int server_fd;
	
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == -1) {
		printf("Socket creation failed: %s...\n", strerror(errno));
		return 1;
	}

	// Since the tester restarts your program quite often, setting SO_REUSEADDR
	// ensures that we don't run into 'Address already in use' errors
	int reuse = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
		printf("SO_REUSEADDR failed: %s \n", strerror(errno));
		return 1;
	}
	
	struct sockaddr_in serv_addr = { .sin_family = AF_INET ,
					 .sin_port = htons(6379),
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
	
	pthread_mutex_init(&accept_mutex, NULL);

	pthread_t thread_pool[MAX_CONCURRENT_CLIENTS];
	for (int i = 0; i < MAX_CONCURRENT_CLIENTS; i++) {
		pthread_t thread;
		pthread_attr_t thread_attr;

		pthread_attr_init(&thread_attr);
		pthread_attr_setscope(&thread_attr, PTHREAD_SCOPE_SYSTEM);

		pthread_create(&thread, &thread_attr, accept_connection, &server_fd);
		thread_pool[i] = thread;

		pthread_attr_destroy(&thread_attr);
	}

	for (int i = 0; i < MAX_CONCURRENT_CLIENTS; i++) {
		pthread_join(thread_pool[i], NULL);
	}

	close(server_fd);

	return 0;
}

char* to_upper(char* src) {
	for (int i = 0; src[i] != '\0'; i++)
		src[i] = toupper(src[i]);
	return src;
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
		/*
		while (read(client_fd, buffer, 1024) > 0) {
		}
		*/
		int n = read(client_fd, buffer, 1024);
		buffer[n] = '\0';

		RespParser* parser = create_resp_parser(buffer);
		size_t num_datas = 0;
		RespData** datas = execute_resp_parser(parser, &num_datas);

		if (num_datas == 0) {
			send(client_fd, "-Bad Request\r\n", 14, 0);
		} else {
			print_resp_data_array(datas, num_datas);

			RespData* command_array = datas[0];
			if (command_array->type == RESP_ARRAY) {
				RespArray* array = command_array->value->array;
				if (array->length > 0) {
					RespData* command = array->data->values[0];
					if (command->type == RESP_BULK_STRING && strcmp(to_upper(command->value->string), "ECHO") == 0) {
						RespData* response = array->data->values[1];
						if (response != NULL && response->type == RESP_BULK_STRING) {
							char message[256];				
							sprintf(message, "$%d\r\n%s\r\n", strlen(response->value->string), response->value->string);
							send(client_fd, message, strlen(message), -1);
						}
					}
				}
			}
		}

		close(client_fd);
	}

	return NULL;
}
