#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#include "storage.h"

KeyValueStore* create_kv_store() {
	KeyValueStore* store = malloc(sizeof(KeyValueStore));

	char** keys = malloc(DEFAULT_STORAGE_CAPACITY * sizeof(char*));
	char** values = malloc(DEFAULT_STORAGE_CAPACITY * sizeof(char*));

	store->keys = keys;
	store->values = values;
	store->length = 0;
	store->capacity = DEFAULT_STORAGE_CAPACITY;

	return store;
}

void free_kv_store(KeyValueStore* store) {
	free(store->keys);
	store->keys = NULL;
	free(store->values);
	store->values = NULL;
	free(store);
	store = NULL;
}

int kv_get_index(KeyValueStore* store, char* key) {
	if (key == NULL)
		return -1;

	for (size_t i = 0; i < store->length; i++)
		if (strcmp(store->keys[i], key) == 0)
			return i;
	return -1;
}

char* kv_get(KeyValueStore* store, char* key) {
	if (key == NULL)
		return NULL;

	int index = kv_get_index(store, key);
	if (index >= 0)
		return store->values[(size_t)index];

	return NULL;
}

bool kv_exists(KeyValueStore* store, char* key) {
	return kv_get_index(store, key) >= 0;
}

bool kv_set(KeyValueStore* store, char* key, char* value) {
	if (key == NULL)
		return false;

	// TODO: realloc here
	if (store->length >= store->capacity)
		return false;

	int index = kv_get_index(store, key);
	if (index >= 0) {
		store->values[(size_t)index] = value;
	} else {
		store->keys[store->length] = key;
		store->values[store->length] = value;
		store->length++;
	}
	return true;
}

bool kv_set_px(KeyValueStore* store, char* key, char* value, int ms) {
	if (key == NULL)
		return false;

	// TODO: realloc here
	if (store->length >= store->capacity)
		return false;

	int index = kv_get_index(store, key);
	if (index >= 0) {
		store->values[(size_t)index] = value;
	} else {
		store->keys[store->length] = key;
		store->values[store->length] = value;
		store->length++;
	}

	ExpireInfo* info = malloc(sizeof(ExpireInfo));
	info->store = store;
	info->key = key;
	info->after = ms;

	pthread_t thread;
	pthread_create(&thread, NULL, kv_expire, info);

	return true;
}

char* kv_delete(KeyValueStore* store, char* key) {
	if (key == NULL)
		return NULL;

	int index = kv_get_index(store, key);
	if (index < 0)
		return NULL;

	char* value = store->values[index];
	store->values[index] = NULL;
	return value;
}

void* kv_expire(void* arg) {
	ExpireInfo* expire = (ExpireInfo*) arg;
	printf("Evicting %s in %dms\n", expire->key, expire->after);
	usleep(expire->after * 1000);
	kv_delete(expire->store, expire->key);
	printf("Evicted %s\n", expire->key);
	free(expire);
	return NULL;
}

void print_kv_store(KeyValueStore* store) {
	printf("KeyValueStore (%d/%d) {\n", store->length, store->capacity);
	for (size_t i = 0; i < store->length; i++)
		printf("\t%d. '%s' = '%s'\n", i + 1, store->keys[i], store->values[i]);
	printf("}\n");
}
