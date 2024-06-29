#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "commands.h"
#include "parser.h"
#include "encoder.h"
#include "utils.h"
#include "server.h"
#include "storage.h"

static __thread char response_buf[65536];

const char* execute_command(RespArray* command_array) {
	if (command_array->length <= 0 || command_array->is_nested)
		return NULL;

	RespData* command_data = command_array->data->values[0];
	assert_resp_string(command_data);

	char* command = command_data->value->string;
	to_upper(command);

	if (strcmp(command, "PING") == 0)
		return ping_command();
	else if (strcmp(command, "ECHO") == 0) {
		assert(command_array->length >= 2);
		return echo_command(command_array->data->values[1]);
	} else if (strcmp(command, "GET") == 0) {
		assert(command_array->length >= 2);
		return get_command(command_array->data->values[1]);
	} else if (strcmp(command, "SET") == 0) {
		assert(command_array->length >= 3);
		return set_command(
			command_array->data->values[1], 
			command_array->data->values[2],
			command_array->length >= 5 ? command_array->data->values[3] : NULL,
			command_array->length >= 5 ? command_array->data->values[4] : NULL
		);
	} else if (strcmp(command, "INFO") == 0) {
		assert(command_array->length >= 2);
		return info_command(command_array->data->values[1]);
	} else if (strcmp(command, "REPLCONF") == 0) {
		assert(command_array->length >= 3);
		return replconf_command(
			command_array->data->values[1], 
			command_array->data->values[2]
		);
	} else if (strcmp(command, "PSYNC") == 0) {
		assert(command_array->length >= 3);
		return psync_command(
			command_array->data->values[1], 
			command_array->data->values[2]
		);
	}

	return NULL;
}

/**
 * Given an string allocated by the encoder, populates the
 * (TLS) response and returns it
 */
const char* to_response_buffer(char* allocd_str) {
	if (allocd_str == NULL) return NULL;

	snprintf(response_buf, sizeof(response_buf), "%s", allocd_str);
	free(allocd_str);
	return response_buf;
}

const char* ping_command() {
	return to_response_buffer(to_bulk_string("PONG"));	
}

const char* echo_command(RespData* echo_data) {
	assert_resp_string(echo_data);
	char* echo_response = echo_data->value->string;
	return to_response_buffer(to_bulk_string(echo_response));
}

const char* get_command(RespData* key_data) {
	assert_resp_string(key_data);
	char* key = key_data->value->string;
	
	char* value = kv_get(global_store, key);
	if (value == NULL)
		return NULL_BULK_STRING;
	return to_response_buffer(to_bulk_string(value));
}

const char* set_command(RespData* key_data, RespData* value_data, RespData* expiration_data, RespData* duration_data) {
	assert_resp_string(key_data);
	assert_resp_string(value_data);

	char* key = strclone(key_data->value->string);
	char* value = strclone(value_data->value->string);

	if (expiration_data != NULL && duration_data != NULL) {
		assert_resp_string(expiration_data);
		assert_resp_string(duration_data);

		char* expiration_type = expiration_data->value->string;	
		to_upper(expiration_type);
		int duration = atoi(duration_data->value->string);

		if (strcmp(expiration_type, "PX") == 0) {
			kv_set_px(global_store, key, value, duration);
		} else {
			return to_response_buffer(to_simple_error("Unsupported expiration type"));
		}

		return OK_RESPONSE;
	} else {
		kv_set(global_store, key, value);
		return OK_RESPONSE;
	}

	return NULL;
}

const char* info_command(RespData* target_data) {
	assert_resp_string(target_data);
	char* target = target_data->value->string;
	to_upper(target);

	if (strcmp(target, "REPLICATION") == 0) {
		char message[256];
		if (global_server->role == REPLICATION_ROLE_MASTER) {
			MasterRedisInfo* info = global_server->info.master;
			sprintf(
				message, 
				"# %s\nrole:%s\nconnected_slaves:%d\nmaster_replid:%s\nmaster_repl_offset:%d\n",
				target, global_server->role, info->num_connected_slaves, info->replid, info->repl_offset
			);
		} else if (global_server->role == REPLICATION_ROLE_SLAVE) {
			SlaveRedisInfo* info = global_server->info.slave;
			char message[256];
			sprintf(
				message, 
				"# %s\nrole:%s\n",
				target, global_server->role
			);
		}
		return to_response_buffer(to_bulk_string(message));
	}

	return to_response_buffer(to_simple_error("Unsupported info target"));
}

char* replconf_port = NULL;
const char* replconf_command(RespData* confkey_data, RespData* value_data) {
	assert_resp_string(confkey_data);
	assert_resp_string(value_data);

	char* confkey = confkey_data->value->string;
	to_upper(confkey);
	char* value = value_data->value->string;
	to_upper(value);

	if (strcmp(confkey, "LISTENING-PORT") == 0) {
		replconf_port = strclone(value);
		printf("REPLCONF: Replica listening on port %s\n", value);
		return OK_RESPONSE;
	} else if (strcmp(confkey, "CAPA") == 0) {
		// TODO: check actual capa, expand replconf_command args to accept RespData**, size_t
		printf("REPLCONF: Replica with capabilities at least %s\n", value);
		free(replconf_port);
		replconf_port = NULL;
		return OK_RESPONSE;
	}

	return to_response_buffer(to_simple_error("Unsupported REPLCONF key"));
}

const char* psync_command(RespData* master_replid_data, RespData* master_offset_data) {
	if (ami_slave())
		return to_response_buffer(to_simple_error("PSYNC command received in replica"));

	MasterRedisInfo* info = global_server->info.master;

	assert_resp_string(master_replid_data);
	assert_resp_string(master_offset_data);

	char* master_replid = master_replid_data->value->string;
	char* master_offset = master_offset_data->value->string;

	if (strcmp(master_replid, "?") == 0 && strcmp(master_offset, "-1") == 0) {
		char message[256];
		sprintf(message, "+FULLRESYNC %s %d\r\n", info->replid, info->repl_offset);

		char response[1280];
		int rdb_fd = open("data/empty.rdb", 0);
		if (rdb_fd >= 0) {
			char buf[1024];
			ssize_t bytes_read = read(rdb_fd, buf, 1024);
			sprintf(response, "%s$%d\r\n%s", message, strlen(buf), buf);
			close(rdb_fd);
		}

		strcpy(response_buf, response);
		return response_buf;
	}

	return to_response_buffer(to_simple_error("Unsupported PSYNC values"));
}
