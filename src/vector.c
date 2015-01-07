// vim: noet ts=4 sw=4
#include <string.h>
#include "vector.h"

vector *vector_new(const size_t item_size, const size_t initial_element_count) {
	vector _vec = {
		.item_size = item_size,
		.max_size = initial_element_count,
		.count = 0,
		.items = calloc(initial_element_count, item_size)
	};

	vector *to_return = calloc(1, sizeof(vector));
	memcpy(to_return, &_vec, sizeof(vector));

	return to_return;
}

int vector_append(vector *vec, const void *item, const size_t item_size) {
	return 1;
}

inline const void *vector_get(const vector *vec, const unsigned int i) {
	if (i > vec->max_size)
		return NULL;
	return NULL;
}

void vector_free(vector *to_free) {
	int i;
	for (i = 0; i < to_free->count; i++) {
		free(to_free->items[i]);
	}
	free(to_free->items);
	free(to_free);
}


