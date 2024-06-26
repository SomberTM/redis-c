#ifndef RESP_ENCODER_H
#define RESP_ENCODER_H

#include "parser.h"

extern const char* NULL_BULK_STRING;
extern const char* OK_RESPONSE;

char* encode_resp_data(RespData*);
char* to_bulk_string(char*);
char* to_simple_error(char*);

#endif /* RESP_ENCODER_H */
