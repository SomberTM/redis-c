#include <stdlib.h>
#include <string.h>

#include "storage.h"

KeyValueStore* create_kv_store() {
	KeyValueStore* store = malloc(sizeof(KeyValueStore));

	char** keys = malloc(DEFAULT_STORAGE_CAPACITY * sizeof(char*));
	void** values = malloc(DEFAULT_STORAGE_CAPACITY * sizeof(void*));

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

size_t kv_get_index(KeyValueStore* store, char* key) {
	if (key == NULL)
		return -1;

	for (size_t i = 0; i < store->length; i++)
		if (strcmp(store->keys[i], key) == 0)
			return i;
	return -1;
}

void* kv_get(KeyValueStore* store, char* key) {
	if (key == NULL)
		return NULL;

	size_t index = kv_get_index(store, key);
	if (index >= 0)
		return store->values[index];

	return NULL;
}

bool kv_exists(KeyValueStore* store, char* key) {
	return kv_get_index(store, key) >= 0;
}

void kv_set(KeyValueStore* store, char* key, void* value) {
	if (key == NULL)
		return;

	// TODO: realloc here
	if (store->length >= store->capacity)
		return;

	size_t index = kv_get_index(store, key);
	if (index >= 0) {
		store->values[index] = value;
	} else {
		store->keys[store->length] = key;
		store->values[store->length] = value;
		store->length++;
	}
}
