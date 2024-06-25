#ifndef RESP_PARSER_H
#define RESP_PARSER_H

#include <stdbool.h>
#include <stddef.h>

#define DEFAULT_RESP_DATA_LENGTH 8

typedef enum {
	RESP_SIMPLE_STRING,
	RESP_SIMPLE_ERROR,
	RESP_INTEGER,
	RESP_BULK_STRING,
	RESP_ARRAY,
	RESP_NULL,
	RESP_BOOLEAN,
	RESP_DOUBLE,
	RESP_BIG_NUMBER,
	RESP_BULK_ERROR,
	RESP_VERBATIM_STRING,
	RESP_MAP,
	RESP_SET,
	RESP_PUSHES,
	RESP_DATA_TYPE_UNKNOWN
} RespDataType;

typedef struct RespArray RespArray;
typedef struct RespData RespData;

typedef union {
	RespData** values;
	RespArray** arrays;
} RespArrayData;

struct RespArray {
	RespArrayData* data;
	size_t length;
	bool is_nested;
};

typedef union {
	char* string;
	int integer;
	double dooble;
	bool boolean;
	RespArray* array;
	/* TODO: create proper struct */
	void* verbatim_string;
	/* TODO: create proper struct */
	void* map;
	/* TODO: create proper struct */
	void* set;
	/* TODO: create proper struct, same type as array */
	RespArray* pushes;
} RespDataValue;

struct RespData {
	RespDataType type;
	RespDataValue* value;
};

typedef struct {
	char** items;
	size_t items_length;
	size_t current_item_index;
} RespParser;

RespDataType get_resp_data_type(char);
RespData* create_resp_data(RespDataType);
RespArray* create_resp_array();
void free_resp_array(RespArray*);

char** split_resp_request(char*, size_t*);

RespParser* create_resp_parser(char*);
void free_resp_parser(RespParser*);
RespData** execute_resp_parser(RespParser*, size_t*);
void free_resp_array(RespArray*);
void free_resp_data_array(RespData**, size_t);

void print_resp_data(RespData*);
void print_resp_data_array(RespData**, size_t);

#endif /* RESP_PARSER_H */
