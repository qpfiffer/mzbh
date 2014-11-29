// vim: noet ts=4 sw=4
#include <stdlib.h>
#include <stdio.h>

#include "models.h"
#include "parson.h"

void create_webm_key(const char *file_hash, char outbuf[MAX_KEY_SIZE]) {
	snprintf(outbuf, MAX_KEY_SIZE, "%s%s%s", WAIFU_NMSPC, WEBM_NMSPC, file_hash);
}

char *serialize_webm(const webm *to_serialize) {
	JSON_Value *root_value = json_value_init_object();
	JSON_Object *root_object = json_value_get_object(root_value);

	char *serialized_string = NULL;

	json_object_set_boolean(root_object, "is_alias", (int)to_serialize->is_alias);

	json_object_set_string(root_object, "file_hash", to_serialize->file_hash);
	json_object_set_string(root_object, "filename", to_serialize->filename);

	json_object_set_number(root_object, "created_at", to_serialize->created_at);
	json_object_set_number(root_object, "size", to_serialize->size);

	const size_t buf_siz = json_serialization_size(root_value);
	serialized_string = calloc(1, buf_siz);
	json_serialize_to_buffer(root_value, serialized_string, buf_siz);

	json_value_free(root_value);
	return serialized_string;
}
