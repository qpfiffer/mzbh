// vim: noet ts=4 sw=4
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <38-moths/parse.h>
#include <38-moths/logging.h>

#include "http.h"
#include "utils.h"
#include "parse.h"
#include "models.h"

int webm_serialization() {
	webm to_test = {
		.file_hash = "47C81F590352D9F95C4CDC4FC7985195D5BD45317728A61791F1D9D9EE530E15",
		._null_term_hax_1 = 0,
		.filename = "walrus_eating_cake.webm",
		._null_term_hax_2 = 0,
		.board = "b",
		._null_term_hax_3 = 0,
		.post = "POST129382983",
		._null_term_hax_4 = 0,
		.created_at = 1419626658,
		.size = 123000
	};

	char *serialized = serialize_webm(&to_test);
	webm *deserialized = deserialize_webm(serialized);

	assert(memcmp(&to_test, deserialized, sizeof(webm)) == 0);
	free(serialized);
	free(deserialized);

	return 1;
}

int webm_alias_serialization() {
	webm_alias to_test = {
		.file_hash = "47C81F590352D9F95C4CDC4FC7985195D5BD45317728A61791F1D9D9EE530E15",
		._null_term_hax_1 = 0,
		.filename = "walrus_eating_cake.webm",
		._null_term_hax_2 = 0,
		.board = "b",
		._null_term_hax_3 = 0,
		.created_at = 1419626658,
		.post = "POST129382983",
		._null_term_hax_4 = 0,
	};

	char *serialized = serialize_alias(&to_test);
	webm_alias *deserialized = deserialize_alias(serialized);

	assert(memcmp(&to_test, deserialized, sizeof(webm_alias)) == 0);
	free(serialized);
	free(deserialized);

	return 1;
}

int thread_serialization() {
	thread to_test = {
		.board = "b",
		._null_term_hax_1 = 0,
		.post_keys = vector_new(sizeof(char) * MAX_KEY_SIZE, 16)
	};

	unsigned int i;
	for (i = 0; i < 3; i++)
		vector_append(to_test.post_keys, "TEST_KEY", strlen("TEST_KEY"));

	char *serialized = serialize_thread(&to_test);
	thread *deserialized = deserialize_thread(serialized);

	assert(memcmp(to_test.board, deserialized->board, sizeof(to_test.board)) == 0);
	assert(to_test.post_keys->count == deserialized->post_keys->count);
	for (i = 0; i < deserialized->post_keys->count; i++)
		assert(strncmp(vector_get(to_test.post_keys, i), vector_get(deserialized->post_keys, i), MAX_KEY_SIZE) == 0);

	vector_free(to_test.post_keys);
	vector_free(deserialized->post_keys);
	free(serialized);
	free(deserialized);

	return 1;
}

int post_serialization() {
	post to_test = {
		.post_id = "145685859304",
		._null_term_hax_1 = 0,
		.thread_key = "THRD",
		._null_term_hax_2 = 0,
		.board = "/b/",
		._null_term_hax_3 = 0,
		.webm_key = "FakeWebmKeyLol",
		._null_term_hax_4 = 0,
		.body_content = malloc(strlen("Hello!")),
		.replied_to_keys = vector_new(sizeof(char) * MAX_KEY_SIZE, 16)
	};
	strncpy(to_test.body_content, "Hello!", strlen("Hello!"));

	unsigned int i;
	for (i = 0; i < 3; i++)
		vector_append(to_test.replied_to_keys, "TEST_KEY", strlen("TEST_KEY"));

	char *serialized = serialize_post(&to_test);
	post *deserialized = deserialize_post(serialized);

	assert(memcmp(to_test.post_id, deserialized->post_id, sizeof(to_test.post_id)) == 0);
	assert(memcmp(to_test.thread_key, deserialized->thread_key, sizeof(to_test.thread_key)) == 0);
	assert(memcmp(to_test.board, deserialized->board, sizeof(to_test.board)) == 0);
	assert(memcmp(to_test.webm_key, deserialized->webm_key, sizeof(to_test.webm_key)) == 0);
	assert(memcmp(to_test.body_content, deserialized->body_content, strlen(to_test.body_content)) == 0);

	assert(to_test.replied_to_keys->count == deserialized->replied_to_keys->count);
	for (i = 0; i < deserialized->replied_to_keys->count; i++)
		assert(strncmp(vector_get(to_test.replied_to_keys, i), vector_get(deserialized->replied_to_keys, i), MAX_KEY_SIZE) == 0);

	vector_free(to_test.replied_to_keys);
	vector_free(deserialized->replied_to_keys);
	free(to_test.body_content);
	free(deserialized->body_content);
	free(serialized);
	free(deserialized);

	return 1;
}

int hash_stuff() {
	char outbuf[HASH_IMAGE_STR_SIZE] = {0};
	char hash_key[MAX_KEY_SIZE] = "3000655_blitzbang2.webm";

	char outbuf1[HASH_IMAGE_STR_SIZE] = {0};
	char hash_key1[MAX_KEY_SIZE] = "3000655_blitzbang3.webm";

	assert(hash_string_fnv1a((unsigned char *)hash_key, strlen(hash_key), outbuf));
	assert(hash_string_fnv1a((unsigned char *)hash_key1, strlen(hash_key1), outbuf1));

	assert(strcmp(outbuf, outbuf1) != 0);
	return 1;
}

int can_get_header_values() {
	const char header[] =
		"HTTP/1.1 200 OK\r\n"
		"Content-Length: 12281\r\n"
		"X-Olegdb-Num-Matches: 178\r\n"
		"Date: Wed, 31 Dec 2014 23:44:08 GMT\r\n"
		"Content-Type: text/plain; charset=utf-8\r\n\r\n";

	char *num_matches = m38_get_header_value_raw(header, strlen(header), "X-Olegdb-Num-Matches");
	assert(num_matches != NULL);
	free(num_matches);

	char *clength = m38_get_header_value_raw(header, strlen(header), "Content-Length");
	assert(clength != NULL);
	free(clength);

	return 1;
}

int can_serialize_w2a() {
	vector *new_vec = vector_new(sizeof(char) * MAX_KEY_SIZE, 2);
	vector_append(new_vec, "test1", strlen("test1") + 1);
	vector_append(new_vec, "test2", strlen("test2") + 1);
	vector_append(new_vec, "test3, rabbit", strlen("test3, rabbit") + 1);

	webm_to_alias _test = {
		.aliases = new_vec
	};

	char *json = serialize_webm_to_alias(&_test);
	m38_log_msg(LOG_FUN, "json: %s", json);
	assert(json);

	webm_to_alias *deserialized = deserialize_webm_to_alias(json);
	assert(deserialized);

	free(json);
	vector_free(new_vec);
	vector_free(deserialized->aliases);
	free(deserialized);
	return 1;
}

int vectors_are_zeroed() {
	char k1[] = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
	vector *new_vec = vector_new(MAX_KEY_SIZE, 2);
	vector_append(new_vec, k1, strlen(k1));
	vector_append(new_vec, k1, strlen(k1));
	vector_append(new_vec, k1, strlen(k1));
	vector_append(new_vec, k1, strlen(k1));
	vector_free(new_vec);

	char k2[] = "12345678901234567890";
	new_vec = vector_new(MAX_KEY_SIZE, 2);
	vector_append(new_vec, k2, strlen(k2));
	vector_append(new_vec, k2, strlen(k2));
	vector_append(new_vec, k2, strlen(k2));
	vector_append(new_vec, k2, strlen(k2));

	unsigned int i;
	for (i = 0; i < new_vec->count; i++)
		assert(strncmp(k2, vector_get(new_vec, i), strlen(k2)) == 0);

	vector_free(new_vec);

	return 1;
}

int can_parse_range_query() {
	const char a[] = "bytes=0-";
	const char b[] = "bytes=0-1234567";
	const char c[] = "bytes=12345-586868";

	m38_range_header a_p = m38_parse_range_header(a);
	assert(a_p.limit == 0 && a_p.offset == 0);

	m38_range_header b_p = m38_parse_range_header(b);
	assert(b_p.limit == 1234567 && b_p.offset == 0);

	m38_range_header c_p = m38_parse_range_header(c);
	assert(c_p.limit == 586868 && c_p.offset == 12345);

	return 1;
}

int run_tests() {
	webm_serialization();
	webm_alias_serialization();
	thread_serialization();
	hash_stuff();
	can_get_header_values();
	can_serialize_w2a();
	vectors_are_zeroed();
	can_parse_range_query();

	return 0;
}

int main(const int argc, const char *argv[]) {
	UNUSED(argc);
	UNUSED(argv);
	return run_tests();
}
