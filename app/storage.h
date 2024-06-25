#ifndef STORAGE_H
#define STORAGE_H

#include <stddef.h>
#include <stdbool.h>

#define DEFAULT_STORAGE_CAPACITY 256

typedef struct {
	char** keys;
	char** values;
	size_t length;
	size_t capacity;
} KeyValueStore;

KeyValueStore* create_kv_store();
void free_kv_store(KeyValueStore*);
void print_kv_store(KeyValueStore*);

char* kv_get(KeyValueStore*, char*);
bool kv_exists(KeyValueStore*, char*);
bool kv_set(KeyValueStore*, char*, char*);


#endif /* STORAGE_H */
