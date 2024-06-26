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
#include "storage.h"
#include "encoder.h"
#include "utils.h"

#define MAX_CONCURRENT_CLIENTS 10

pthread_mutex_t accept_mutex;
KeyValueStore* global_store;

void* accept_connection(void*);

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

	int master_port = 6379;
	if (argc >= 3) {
		for (int i = 0; i < argc; i++) {
			char* arg = argv[i];
			if (strcmp(arg, "--port") == 0)
				master_port = atoi(argv[i + 1]);
		}
	}
	
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
					 .sin_port = htons(master_port),
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

	printf("Listening on port %d\n", master_port);

	global_store = create_kv_store();
	
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

	free_kv_store(global_store);
	global_store = NULL;

	close(server_fd);

	return 0;
}

char* to_upper(char* src) {
	for (int i = 0; src[i] != '\0'; i++)
		src[i] = toupper(src[i]);
	return src;
}

void raw_print(char* str) {
	while (*str) {
		if (*str == '\r') {
			printf("\\r");
		} else if (*str == '\n') {
			printf("\\n");
		} else {
			putchar(*str);
		}
		str++;
	}
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

			bool sent = false;

			if (num_datas == 0) {
				send(client_fd, "-Bad Request\r\n", 14, 0);
				sent = true;
			} else {
				print_resp_data_array(datas, num_datas);

				RespData* command_array = datas[0];
				if (command_array->type == RESP_ARRAY) {
					RespArray* array = command_array->value->array;
					if (array->length > 0) {
						RespData* command = array->data->values[0];
						if (command->type == RESP_BULK_STRING) {
							to_upper(command->value->string);

							if (strcmp(command->value->string, "PING") == 0) {
								send(client_fd, "+PONG\r\n", 7, 0);
							} else if (strcmp(command->value->string, "ECHO") == 0) {
								RespData* echo = array->data->values[1];
								if (echo != NULL && echo->type == RESP_BULK_STRING) {
									char* response = encode_resp_data(echo);
									send(client_fd, response, strlen(response), 0);
									free(response);
								}
							} else if (strcmp(command->value->string, "GET") == 0) {
								RespData* key = array->data->values[1];
								char* value = kv_get(global_store, key->value->string);
								if (value == NULL) {
									send(client_fd, NULL_BULK_STRING, strlen(NULL_BULK_STRING), 0);
								} else {
									char* response = to_bulk_string(value);
									send(client_fd, response, strlen(response), 0);
									free(response);
								}
							} else if (strcmp(command->value->string, "SET") == 0) {
								RespData* key = array->data->values[1];
								RespData* value = array->data->values[2];

								char* error = "Invalid arguments for \"SET\" command";
								if (key == NULL || value == NULL) {
									send(client_fd, error, strlen(error), 0);
								} else {
									RespData* expiration = array->data->values[3];
									RespData* expire_after = array->data->values[4];

									if (expiration != NULL && expire_after != NULL) {
										char* expiration_type = expiration->value->string;
										to_upper(expiration_type);
										
										if (strcmp(expiration_type, "PX") == 0) {
											int expires_after = atoi(expire_after->value->string);
											if (kv_set_px(global_store, strclone(key->value->string), strclone(value->value->string), expires_after))
												send(client_fd, OK_RESPONSE, strlen(OK_RESPONSE), 0);
										}
									} else if (kv_set(
										global_store, 
										strclone(key->value->string), 
										strclone(value->value->string)
									)) {
										send(client_fd, OK_RESPONSE, strlen(OK_RESPONSE), 0);
									} else {
										error = "-Failed to \"SET\"\r\n";
										send(client_fd, error, strlen(error), 0);
									}
								}
							} else if (strcmp(command->value->string, "INFO") == 0) {
								RespData* info_of = array->data->values[1];
								if (info_of == NULL) {
									char* response = to_simple_error("Invalid INFO usage");
									send(client_fd, response, strlen(response), 0);
									free(response);
								} else {
									char* response = to_bulk_string("# Replication\nrole:master");
									send(client_fd, response, strlen(response), 0);
									free(response);
								}
							} else {
								char message[256];
								sprintf(message, "-\"%s\" command not supported\r\n", command->value->string);
								send(client_fd, message, strlen(message), 0);
							}

							sent = true;
						}
					}
				}
			}

			if (!sent) {
				printf("No message was sent\n");
				send(client_fd, "-Bad Request\r\n", 14, 0);
			}

			free_resp_data_array(datas, num_datas);
			print_kv_store(global_store);
		}

		close(client_fd);
	}

	return NULL;
}
