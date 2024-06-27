#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "commands.h"
#include "parser.h"
#include "encoder.h"
#include "utils.h"
#include "server.h"
#include "storage.h"

char* execute_command(RespArray* command_array) {
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
	}

	return NULL;
}

char* ping_command() {
	return to_bulk_string("PONG");
}

char* echo_command(RespData* echo_data) {
	assert_resp_string(echo_data);
	char* echo_response = echo_data->value->string;
	return to_bulk_string(echo_response);
}

char* get_command(RespData* key_data) {
	assert_resp_string(key_data);
	char* key = key_data->value->string;
	
	char* value = kv_get(global_store, key);
	if (value == NULL)
		return NULL_BULK_STRING;
	return to_bulk_string(value);
}

char* set_command(RespData* key_data, RespData* value_data, RespData* expiration_data, RespData* duration_data) {
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
			return to_simple_error("Unsupported expiration type");
		}

		return OK_RESPONSE;
	} else {
		kv_set(global_store, key, value);
		return OK_RESPONSE;
	}

	return NULL;
}

char* info_command(RespData* target_data) {
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
				target, global_server->role, info->connected_slaves, info->replid, info->repl_offset
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
		return to_bulk_string(message);
	}

	return to_simple_error("Unsupported info target");
}
