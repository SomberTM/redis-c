#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include "utils.h"

char* strclone(const char* src) {
	size_t len = strlen(src);
	char* dest = (char*) malloc(len * sizeof(char));

	strcpy(dest, src);
	return dest;
}
