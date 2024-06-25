#include "parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

RespDataType get_resp_data_type(char first_byte) {
	switch (first_byte) {
		case '+': return RESP_SIMPLE_STRING;
		case '-': return RESP_SIMPLE_ERROR;
		case ':': return RESP_INTEGER;
		case '$': return RESP_BULK_STRING;
		case '*': return RESP_ARRAY;
		case '_': return RESP_NULL;
		case '#': return RESP_BOOLEAN;
		case ',': return RESP_DOUBLE;
		case '(': return RESP_BIG_NUMBER;
		case '!': return RESP_BULK_ERROR;
		case '=': return RESP_VERBATIM_STRING;
		case '%': return RESP_MAP;
		case '~': return RESP_SET;
		case '>': return RESP_PUSHES;
		default:  return RESP_DATA_TYPE_UNKNOWN;
	}
} 

RespData* create_resp_data(RespDataType type) {
	RespData* data = (RespData*) malloc(sizeof(RespData));
	RespDataValue* value = (RespDataValue*) malloc(sizeof(RespDataValue*));
	data->value = value;
	data->type = type;
	return data;
}

RespArray* create_resp_array() {
	RespArray* array = (RespArray*) malloc(sizeof(RespArray));
	RespArrayData* data = (RespArrayData*) malloc(sizeof(RespArrayData));
	array->length = 0;
	array->is_nested = false;
	array->data = data;
	return array;
}

/**
 * Just split the resp datas here, more efficient ways but this makes
 * things easier
 */
char** split_resp_request(char* request, size_t* out_num_strs) {
	int n, k;
	n = 0; k = 1;

	char* temp_strs[256] = { NULL };
	*out_num_strs = 0;

	char buffer[1024] = { '\0' };  
	size_t buf_len = 0;
	for (; request[k] != '\0'; n++, k++) {
		if (request[n] == '\r' && request[k] == '\n') {
			char* temp_str = malloc(buf_len * sizeof(char));
			strncpy(temp_str, buffer, buf_len);

			temp_strs[*out_num_strs] = temp_str;
			*out_num_strs += 1;

			buf_len = 0;
			n++; k++;
			continue;
		}

		buffer[buf_len] = request[n];
		buf_len++;
	}

	char** out = malloc((*out_num_strs) * sizeof(char*));
	for (size_t i = 0; i < *out_num_strs; i++)
		out[i] = temp_strs[i];
	return out;
}

RespParser* create_resp_parser(char* request) {
	RespParser* parser = malloc(sizeof(RespParser));

	RespData** datas = (RespData**) malloc(DEFAULT_RESP_DATA_LENGTH * sizeof(RespData*));
	parser->datas = datas;
	parser->datas_length = 0;

	size_t items_length = 0;
	char** items = split_resp_request(request, &items_length);
	parser->items = items;
	parser->items_length = items_length;
	parser->current_item_index = 0;

	return parser;
}

RespData* get_current_resp_data(RespParser*);

RespData* get_current_bulk_string(RespParser* parser) {
	char* current = parser->items[parser->current_item_index];
	if (get_resp_data_type(current[0]) != RESP_BULK_STRING)
		return NULL;

	int length = atoi(current + 1);
	
	RespData* data = create_resp_data(RESP_BULK_STRING);
	char* string = parser->items[++parser->current_item_index];
	data->value->string = string;

	return data;
}

RespArray* get_current_resp_array(RespParser* parser) {
	char* current = parser->items[parser->current_item_index];
	if (get_resp_data_type(current[0]) != RESP_ARRAY)
		return NULL;

	int array_length = atoi(current + 1);
	
	RespArray* array = create_resp_array();
	if (array_length <= 0)
		return array;

	parser->current_item_index++;

	RespData** datas = (RespData**) malloc(array_length * sizeof(RespData*));
	for(int i = 0; i < array_length; i++) {
		RespData* data = get_current_resp_data(parser);	
		datas[i] = data;
	}

	array->length = array_length;
	array->data->values = datas;

	return array;
}

RespData* get_current_resp_data(RespParser* parser) {
	char* current = parser->items[parser->current_item_index];
	RespDataType type = get_resp_data_type(current[0]);
	char* value = current + 1;

	RespData* data = NULL;
	RespArray* array = NULL;

	switch (type) {
		case RESP_INTEGER:
			data = create_resp_data(RESP_INTEGER);
			data->value->integer = atoi(value);
			break;
		case RESP_BULK_STRING:
			data = get_current_bulk_string(parser);
			break;
		case RESP_ARRAY:
			array = get_current_resp_array(parser);
			data = create_resp_data(RESP_ARRAY);
			data->value->array = array;
			break;
		default:
			data = create_resp_data(type);	
			data->value->string = value;
			break;
	}

	parser->current_item_index++;
	return data;
}

RespData** execute_resp_parser(RespParser* parser, size_t* datas_length) {
	while (parser->current_item_index < parser->items_length) {
		parser->datas[parser->datas_length++] = get_current_resp_data(parser);
	}

	*datas_length = parser->datas_length;
	return parser->datas;
}

void print_resp_array(RespArray* array) {
	// ignoring nested
	printf("Array (%d) [", array->length);
	for (size_t i = 0; i < array->length; i++) {
		RespData* data = array->data->values[i];
		print_resp_data(data);
		if (i < array->length - 1)
			printf(", ");
	}
	printf("]\n");
}

void print_resp_data(RespData* data) {
	switch (data->type) {
		case RESP_ARRAY:
			print_resp_array(data->value->array);
			break;
		default:
			printf("%s", data->value->string);
			break;
	}
}

void print_resp_data_array(RespData** array, size_t len) {
	for (size_t i = 0; i < len; i++) {
		print_resp_data(array[i]);
		printf("\n");
	}
}
