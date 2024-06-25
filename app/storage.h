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
bool kv_set_px(KeyValueStore*, char*, char*, int);
bool kv_set_ex(KeyValueStore*, char*, char*, int);
char* kv_delete(KeyValueStore*, char*);

typedef struct {
	KeyValueStore* store;
	char* key;
	int after;
} ExpireInfo;

void* kv_expire(void*);

#endif /* STORAGE_H */
