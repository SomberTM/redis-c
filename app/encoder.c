#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "encoder.h"

const char* NULL_BULK_STRING = "$-1\r\n";
const char* OK_RESPONSE = "+OK\r\n";

char* encode_resp_data(RespData* data) {
	switch (data->type) {
		case RESP_BULK_STRING:
		case RESP_SIMPLE_STRING:
		case RESP_SIMPLE_ERROR:
			return to_bulk_string(data->value->string);
		default:
			return "-failed to encode\r\n";
	}

	return NULL;
}

char* to_bulk_string(char* buf) {
	if (buf == NULL)
		return NULL;

	size_t buf_len = strlen(buf);
	char* bulk_string = malloc((buf_len + 16) * sizeof(char));

	sprintf(bulk_string, "$%d\r\n%s\r\n", buf_len, buf);
	return bulk_string;
}

char* to_simple_error(char* message) {
	if (message == NULL)
		return NULL;

	size_t message_len = strlen(message);
	char* simple_error = malloc((message_len + 3) * sizeof(char));

	sprintf(simple_error, "-%s\r\n", message);
	return simple_error;
}
