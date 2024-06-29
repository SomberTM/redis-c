#ifndef UTILS_H
#define UTILS_H

#include "parser.h"

char* strclone(const char*);
void assert_resp_string(RespData*);
char* to_upper(char*);
void raw_print(char*);

#endif /* UTILS_H */
