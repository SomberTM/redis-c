#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdio.h>

#include "utils.h"
#include "parser.h"

char* strclone(const char* src) {
	size_t len = strlen(src);
	char* dest = (char*) malloc(len * sizeof(char));

	strcpy(dest, src);
	return dest;
}

char* to_upper(char* src) {
	for (int i = 0; src[i] != '\0'; i++)
		src[i] = toupper(src[i]);
	return src;
}

void assert_resp_string(RespData* data) {
	assert(data->type == RESP_BULK_STRING || data->type == RESP_SIMPLE_STRING);
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

