// vim: noet ts=4 sw=4
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <38-moths/logging.h>
#include <oleg-http/oleg-http.h>

#include "db.h"
#include "models.h"
#include "parson.h"
#include "utils.h"

void create_webm_key(const char file_hash[static HASH_IMAGE_STR_SIZE], char outbuf[static MAX_KEY_SIZE]) {
	snprintf(outbuf, MAX_KEY_SIZE, "%s%s", WEBM_NMSPC, file_hash);
}

webm *deserialize_webm_from_tuples(const PGresult *res) {
	if (!res)
		return NULL;

	if (PQntuples(res) <= 0) {
		m38_log_msg(LOG_WARN, "No tuples in PG result for webm_from_tuples.");
		return NULL;
	}

	webm *to_return = calloc(1, sizeof(webm));

	const int file_hash_col = PQfnumber(res, "file_hash");
	const int filename_col = PQfnumber(res, "filename");
	const int board_col = PQfnumber(res, "board");
	const int file_path_col = PQfnumber(res, "file_path");
	const int size_col = PQfnumber(res, "size");
	const int post_id_col = PQfnumber(res, "post_id");
	const int id_col = PQfnumber(res, "id");
	const int created_at_col = PQfnumber(res, "created_at");

	strncpy(to_return->file_hash, PQgetvalue(res, 0, file_hash_col), sizeof(to_return->file_hash));
	strncpy(to_return->file_path, PQgetvalue(res, 0, file_path_col), sizeof(to_return->file_path));
	strncpy(to_return->filename, PQgetvalue(res, 0, filename_col), sizeof(to_return->filename));
	strncpy(to_return->board, PQgetvalue(res, 0, board_col), sizeof(to_return->board));
	to_return->size = (size_t)atol(PQgetvalue(res, 0, size_col));
	to_return->post_id = (unsigned int)atol(PQgetvalue(res, 0, post_id_col));
	to_return->created_at = (time_t)atol(PQgetvalue(res, 0, created_at_col));
	to_return->id = (unsigned int)atol(PQgetvalue(res, 0, id_col));


	return to_return;
}

webm_alias *deserialize_alias_from_tuples(const PGresult *res, const unsigned int idx) {
	if (!res)
		return NULL;

	if (PQntuples(res) <= 0) {
		//m38_log_msg(LOG_WARN, "No tuples in PG result for alias_from_tuples.");
		return NULL;
	}

	webm_alias *to_return = calloc(1, sizeof(webm));

	const int file_hash_col = PQfnumber(res, "file_hash");
	const int filename_col = PQfnumber(res, "filename");
	const int board_col = PQfnumber(res, "board");
	const int file_path_col = PQfnumber(res, "file_path");
	const int post_id_col = PQfnumber(res, "post_id");
	const int id_col = PQfnumber(res, "id");
	const int webm_id_col = PQfnumber(res, "webm_id");
	const int created_at_col = PQfnumber(res, "created_at");

	strncpy(to_return->file_hash, PQgetvalue(res, idx, file_hash_col), sizeof(to_return->file_hash));
	strncpy(to_return->file_path, PQgetvalue(res, idx, file_path_col), sizeof(to_return->file_path));
	strncpy(to_return->filename, PQgetvalue(res, idx, filename_col), sizeof(to_return->filename));
	strncpy(to_return->board, PQgetvalue(res, idx, board_col), sizeof(to_return->board));
	to_return->post_id = (unsigned int)atol(PQgetvalue(res, idx, post_id_col));
	to_return->webm_id = (unsigned int)atol(PQgetvalue(res, idx, webm_id_col));
	to_return->created_at = (time_t)atol(PQgetvalue(res, idx, created_at_col));
	to_return->id = (unsigned int)atol(PQgetvalue(res, idx, id_col));

	return to_return;
}


post *deserialize_post_from_tuples(const PGresult *res, const unsigned int idx) {
	if (!res)
		return NULL;

	if (PQntuples(res) <= 0) {
		m38_log_msg(LOG_WARN, "No tuples in PG result for post_from_tuples.");
		return NULL;
	}

	struct post *to_return = calloc(1, sizeof(struct post));

	const int fourchan_post_id_col = PQfnumber(res, "fourchan_post_id");
	const int fourchan_post_no_col = PQfnumber(res, "fourchan_post_no");
	const int id_col = PQfnumber(res, "id");
	const int oleg_key_col = PQfnumber(res, "oleg_key");
	const int thread_id_col = PQfnumber(res, "thread_id");
	const int board_col = PQfnumber(res, "board");
	const int body_col = PQfnumber(res, "body_content");
	const int replied_to_keys_col = PQfnumber(res, "replied_to_keys");
	const int created_at_col = PQfnumber(res, "created_at");

	to_return->fourchan_post_id = atol(PQgetvalue(res, idx, fourchan_post_id_col));
	to_return->fourchan_post_no = atol(PQgetvalue(res, idx, fourchan_post_no_col));
	to_return->thread_id = atol(PQgetvalue(res, idx, thread_id_col));
	to_return->id = atol(PQgetvalue(res, idx, id_col));
	to_return->created_at = (time_t)atol(PQgetvalue(res, idx, created_at_col));

	strncpy(to_return->oleg_key, PQgetvalue(res, idx, oleg_key_col), sizeof(to_return->oleg_key));
	strncpy(to_return->board, PQgetvalue(res, idx, board_col), sizeof(to_return->board));

	const char *b_content = PQgetvalue(res, idx, body_col);
	if (b_content) {
		to_return->body_content = strdup(b_content);
	} else {
		to_return->body_content = NULL;
	}

	const char *r_content = PQgetvalue(res, idx, replied_to_keys_col);
	if (r_content) {
		/* TODO: When I have more than one hand, extract this into a function that does
		 * vector -> json and vice versa.
		 */
		JSON_Value *serialized = json_parse_string(r_content);
		JSON_Array *replied_to_keys_array = json_value_get_array(serialized);

		const size_t num_keys  = json_array_get_count(replied_to_keys_array);
		to_return->replied_to_keys = vector_new(MAX_KEY_SIZE, num_keys);

		unsigned int i;
		for (i = 0; i < num_keys; i++) {
			const char *key = json_array_get_string(replied_to_keys_array, i);
			vector_append(to_return->replied_to_keys, key, strlen(key));
		}

		json_value_free(serialized);
	} else
		to_return->replied_to_keys = NULL;

	return to_return;
}

thread *deserialize_thread_from_tuples(const PGresult *res, const unsigned int idx) {
	if (!res)
		return NULL;

	if (PQntuples(res) <= 0) {
		m38_log_msg(LOG_WARN, "No tuples in PG result for thread_from_tuples.");
		return NULL;
	}

	thread *to_return = calloc(1, sizeof(struct thread));

	const int oleg_key_col = PQfnumber(res, "oleg_key");
	const int board_col = PQfnumber(res, "board");
	const int id_col = PQfnumber(res, "id");
	const int created_at_col = PQfnumber(res, "created_at");
	const int subject_col = PQfnumber(res, "replied_to_keys");

	to_return->id = atol(PQgetvalue(res, idx, id_col));
	to_return->created_at = (time_t)atol(PQgetvalue(res, idx, created_at_col));

	strncpy(to_return->oleg_key, PQgetvalue(res, idx, oleg_key_col), sizeof(to_return->oleg_key));
	strncpy(to_return->board, PQgetvalue(res, idx, board_col), sizeof(to_return->board));

	const char *b_content = PQgetvalue(res, idx, subject_col);
	if (b_content) {
		to_return->subject = strdup(b_content);
	} else {
		to_return->subject = NULL;
	}

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
	json_object_set_string(root_object, "file_path", to_serialize->file_path);

	//if (to_serialize->post)
	//	json_object_set_string(root_object, "post", to_serialize->post);

	json_object_set_number(root_object, "created_at", to_serialize->created_at);
	json_object_set_number(root_object, "size", to_serialize->size);

	serialized_string = json_serialize_to_string(root_value);
	json_value_free(root_value);
	return serialized_string;
}

static unsigned int x_count(const char query[static MAX_KEY_SIZE]) {
	unsigned int num = get_record_count_in_table(query);
	return num;
}

unsigned int webm_count() {
	char query[MAX_KEY_SIZE] = "SELECT count(*) FROM webms;";
	return x_count(query);
}

unsigned int webm_alias_count() {
	char query[MAX_KEY_SIZE] = "SELECT count(*) FROM webm_aliases;";
	return x_count(query);
}

unsigned int post_count() {
	char query[MAX_KEY_SIZE] = "SELECT count(*) FROM posts;";
	return x_count(query);
}

void create_alias_key(const char file_path[static MAX_IMAGE_FILENAME_SIZE], char outbuf[static MAX_KEY_SIZE]) {
	/* MORE HASHES IS MORE POWER */
	char str_hash[HASH_IMAGE_STR_SIZE] = {0};
	hash_string((unsigned char *)file_path, strnlen(file_path, MAX_IMAGE_FILENAME_SIZE), str_hash);

	char second_hash[HASH_IMAGE_STR_SIZE] = {0};
	hash_string_fnv1a((unsigned char *)file_path, strnlen(file_path, MAX_IMAGE_FILENAME_SIZE), second_hash);

	snprintf(outbuf, MAX_KEY_SIZE, "%s%s%s", ALIAS_NMSPC, str_hash, second_hash);
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
	json_object_set_string(root_object, "file_path", to_serialize->file_path);

	// if (to_serialize->post)
	// 	json_object_set_string(root_object, "post", to_serialize->post);

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
	strncpy(to_return->file_path, json_object_get_string(webm_alias_object, "file_path"), sizeof(to_return->file_path));

	// const char *post = json_object_get_string(webm_alias_object, "post");
	// if (post != NULL)
	// 	strncpy(to_return->post, post, sizeof(to_return->post));

	to_return->created_at = (time_t)json_object_get_number(webm_alias_object, "created_at");
	to_return->id = json_object_get_number(webm_alias_object, "id");

	json_value_free(serialized);
	return to_return;
}

void create_thread_key(const char board[static MAX_BOARD_NAME_SIZE], const char *thread_id,
		char outbuf[static MAX_KEY_SIZE]) {
	snprintf(outbuf, MAX_KEY_SIZE, "%s%s%s", THREAD_NMSPC, board, thread_id);
}

char *serialize_thread(const thread *to_serialize) {
	if (!to_serialize)
		return NULL;

	JSON_Value *root_value = json_value_init_object();
	JSON_Object *root_object = json_value_get_object(root_value);

	char *serialized_string = NULL;

	json_object_set_string(root_object, "board", to_serialize->board);

	// JSON_Value *post_keys = json_value_init_array();
	// JSON_Array *post_keys_array = json_value_get_array(post_keys);
	//
	// unsigned int i;
	// for (i = 0; i < to_serialize->post_keys->count; i++)
	// 	json_array_append_string(post_keys_array, vector_get(to_serialize->post_keys, i));
	//
	// json_object_set_value(root_object, "post_keys", post_keys);

	serialized_string = json_serialize_to_string(root_value);

	json_value_free(root_value);
	return serialized_string;
}

thread *deserialize_thread(const char *json) {
	if (!json)
		return NULL;

	thread *to_return = calloc(1, sizeof(thread));

	JSON_Value *serialized = json_parse_string(json);
	JSON_Object *thread_object = json_value_get_object(serialized);

	strncpy(to_return->board, json_object_get_string(thread_object, "board"), sizeof(to_return->board));

	// JSON_Array *post_keys_array = json_object_get_array(thread_object, "post_keys");
	//
	// const size_t num_post_keys  = json_array_get_count(post_keys_array);
	// to_return->post_keys = vector_new(MAX_KEY_SIZE, num_post_keys);
	//
	// unsigned int i;
	// for (i = 0; i < num_post_keys; i++) {
	// 	const char *key = json_array_get_string(post_keys_array, i);
	// 	vector_append(to_return->post_keys, key, strlen(key));
	// }

	json_value_free(serialized);
	return to_return;
}

void create_post_key(const char board[static MAX_BOARD_NAME_SIZE], const char *post_id,
	char outbuf[static MAX_KEY_SIZE]) {
	snprintf(outbuf, MAX_KEY_SIZE, "%s%s%s", POST_NMSPC, board, post_id);
}

char *serialize_post(const post *to_serialize) {
	if (!to_serialize)
		return NULL;

	JSON_Value *root_value = json_value_init_object();
	JSON_Object *root_object = json_value_get_object(root_value);

	char *serialized_string = NULL;

	json_object_set_number(root_object, "post_id", to_serialize->fourchan_post_id);
	json_object_set_number(root_object, "thread_id", to_serialize->thread_id);
	json_object_set_string(root_object, "board", to_serialize->board);

	// if (to_serialize->webm_key)
	// 	json_object_set_string(root_object, "webm_key", to_serialize->webm_key);

	if (to_serialize->fourchan_post_no)
		json_object_set_number(root_object, "post_no", to_serialize->fourchan_post_no);

	if (to_serialize->body_content)
		json_object_set_string(root_object, "body_content", to_serialize->body_content);

	JSON_Value *thread_keys = json_value_init_array();
	JSON_Array *thread_keys_array = json_value_get_array(thread_keys);

	unsigned int i;
	for (i = 0; i < to_serialize->replied_to_keys->count; i++)
		json_array_append_string(thread_keys_array,
				vector_get(to_serialize->replied_to_keys, i));

	json_object_set_value(root_object, "replied_to_keys", thread_keys);

	serialized_string = json_serialize_to_string(root_value);

	json_value_free(root_value);
	return serialized_string;
}

post *deserialize_post(const char *json) {
	if (!json)
		return NULL;

	post *to_return = calloc(1, sizeof(post));

	JSON_Value *serialized = json_parse_string(json);
	JSON_Object *post_object = json_value_get_object(serialized);

	to_return->fourchan_post_id = json_object_get_number(post_object, "post_id");
	to_return->thread_id = json_object_get_number(post_object, "thread_id");

	strncpy(to_return->board, json_object_get_string(post_object, "board"), sizeof(to_return->board));

	// const char *wk = json_object_get_string(post_object, "webm_key");
	// if (wk)
	// 	strncpy(to_return->webm_key, json_object_get_string(post_object, "webm_key"), sizeof(to_return->webm_key));

	const char *p_no = json_object_get_string(post_object, "post_no");
	if (p_no)
		to_return->fourchan_post_no = json_object_get_number(post_object, "post_no");

	const char *b_content = json_object_get_string(post_object, "body_content");
	if (b_content) {
		to_return->body_content = strdup(b_content);
	} else {
		to_return->body_content = NULL;
	}

	JSON_Array *post_keys_array = json_object_get_array(post_object, "replied_to_keys");

	const size_t num_keys  = json_array_get_count(post_keys_array);
	to_return->replied_to_keys = vector_new(MAX_KEY_SIZE, num_keys);

	unsigned int i;
	for (i = 0; i < num_keys; i++) {
		const char *key = json_array_get_string(post_keys_array, i);
		vector_append(to_return->replied_to_keys, key, strlen(key));
	}

	json_value_free(serialized);
	return to_return;
}
