// vim: noet ts=4 sw=4
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "models.h"
#include "parson.h"

void create_webm_key(const char file_hash[static HASH_IMAGE_STR_SIZE], char outbuf[static MAX_KEY_SIZE]) {
	snprintf(outbuf, MAX_KEY_SIZE, "%s%s", WEBM_NMSPC, file_hash);
}

webm *deserialize_webm(const char *json) {
	if (!json)
		return NULL;

	webm *to_return = calloc(1, sizeof(webm));

	JSON_Value *serialized = json_parse_string(json);
	JSON_Object *webm_object = json_value_get_object(serialized);

	strncpy(to_return->file_hash, json_object_get_string(webm_object, "file_hash"), sizeof(to_return->file_hash));
	strncpy(to_return->filename, json_object_get_string(webm_object, "filename"), sizeof(to_return->filename));
	strncpy(to_return->board, json_object_get_string(webm_object, "board"), sizeof(to_return->board));

	to_return->created_at = (time_t)json_object_get_number(webm_object, "created_at");
	to_return->size = (size_t)json_object_get_number(webm_object, "size");

	json_value_free(serialized);
	return to_return;
}

char *serialize_webm(const webm *to_serialize) {
	if (!to_serialize)
		return NULL;

	JSON_Value *root_value = json_value_init_object();
	JSON_Object *root_object = json_value_get_object(root_value);

	char *serialized_string = NULL;

	/* json_object_set_boolean(root_object, "is_alias", (int)to_serialize->is_alias); */

	json_object_set_string(root_object, "file_hash", to_serialize->file_hash);
	json_object_set_string(root_object, "filename", to_serialize->filename);
	json_object_set_string(root_object, "board", to_serialize->board);

	json_object_set_number(root_object, "created_at", to_serialize->created_at);
	json_object_set_number(root_object, "size", to_serialize->size);

	serialized_string = json_serialize_to_string(root_value);
	json_value_free(root_value);
	return serialized_string;
}

void create_alias_key(const char filename[static MAX_IMAGE_FILENAME_SIZE], char outbuf[static MAX_KEY_SIZE]) {
	snprintf(outbuf, MAX_KEY_SIZE, "%s%s", ALIAS_NMSPC, filename);
}

char *serialize_alias(const webm_alias *to_serialize) {
	if (!to_serialize)
		return NULL;

	JSON_Value *root_value = json_value_init_object();
	JSON_Object *root_object = json_value_get_object(root_value);

	char *serialized_string = NULL;

	json_object_set_string(root_object, "file_hash", to_serialize->file_hash);
	json_object_set_string(root_object, "filename", to_serialize->filename);
	json_object_set_string(root_object, "board", to_serialize->board);

	json_object_set_number(root_object, "created_at", to_serialize->created_at);

	serialized_string = json_serialize_to_string(root_value);

	json_value_free(root_value);
	return serialized_string;
}

webm_alias *deserialize_alias(const char *json) {
	if (!json)
		return NULL;

	webm_alias *to_return = calloc(1, sizeof(webm_alias));

	JSON_Value *serialized = json_parse_string(json);
	JSON_Object *webm_alias_object = json_value_get_object(serialized);

	strncpy(to_return->file_hash, json_object_get_string(webm_alias_object, "file_hash"), sizeof(to_return->file_hash));
	strncpy(to_return->filename, json_object_get_string(webm_alias_object, "filename"), sizeof(to_return->filename));
	strncpy(to_return->board, json_object_get_string(webm_alias_object, "board"), sizeof(to_return->board));

	to_return->created_at = (time_t)json_object_get_number(webm_alias_object, "created_at");

	json_value_free(serialized);
	return to_return;
}
