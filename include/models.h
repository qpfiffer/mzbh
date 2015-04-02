// vim: noet ts=4 sw=4
#pragma once
#include <38-moths/vector.h>
#include <time.h>
#include "common_defs.h"

/* The unsigned chars in the struct are used to null terminate the
 * strings while still allowing us to use 'sizeof(webm.file_hash)'.
 */
typedef struct webm {
	char file_hash[HASH_IMAGE_STR_SIZE];
	unsigned char _null_term_hax_1;

	char filename[MAX_IMAGE_FILENAME_SIZE];
	unsigned char _null_term_hax_2;

	char board[MAX_BOARD_NAME_SIZE];
	unsigned char _null_term_hax_3;

	char file_path[MAX_IMAGE_FILENAME_SIZE];
	unsigned char _null_term_hax_4;

	char post[MAX_KEY_SIZE];
	unsigned _null_term_hax_5;

	time_t created_at;
	size_t size;
} __attribute__((__packed__)) webm;

void create_webm_key(const char file_hash[static HASH_IMAGE_STR_SIZE], char outbuf[static MAX_KEY_SIZE]);
char *serialize_webm(const webm *to_serialize);
webm *deserialize_webm(const char *json);
unsigned int webm_count();

/* Aliases of webms have the same file hash but different names, boards, etc. Mostly metadata. */
typedef struct webm_alias {
	char file_hash[HASH_IMAGE_STR_SIZE];
	unsigned char _null_term_hax_1;

	char filename[MAX_IMAGE_FILENAME_SIZE];
	unsigned char _null_term_hax_2;

	char board[MAX_BOARD_NAME_SIZE];
	unsigned char _null_term_hax_3;

	char file_path[MAX_IMAGE_FILENAME_SIZE];
	unsigned char _null_term_hax_4;

	char post[MAX_KEY_SIZE];
	unsigned _null_term_hax_5;

	time_t created_at;
} __attribute__((__packed__)) webm_alias;

void create_alias_key(const char file_path[static MAX_IMAGE_FILENAME_SIZE], char outbuf[static MAX_KEY_SIZE]);
char *serialize_alias(const webm_alias *to_serialize);
webm_alias *deserialize_alias(const char *json);
unsigned int webm_alias_count();

/* This is a one-to-many from webm objects to alias objects. The struct is
 * something akin to a vector. Probably. */
typedef struct webm_to_alias {
	vector *aliases;
} webm_to_alias;

void create_webm_to_alias_key(const char file_hash[static HASH_IMAGE_STR_SIZE], char outbuf[static MAX_KEY_SIZE]);
char *serialize_webm_to_alias(const webm_to_alias *w2a);
webm_to_alias *deserialize_webm_to_alias(const char *json);

typedef struct thread {
	char board[MAX_BOARD_NAME_SIZE];
	unsigned char _null_term_hax_1;

	vector *post_keys;
} __attribute__((__packed__)) thread;

void create_thread_key(const char board[static MAX_BOARD_NAME_SIZE], const char *thread_id,
		char outbuf[static MAX_KEY_SIZE]);
char *serialize_thread(const thread *to_serialize);
thread *deserialize_thread(const char *json);

typedef struct post {
	unsigned int post_id; /* 4chan post id. */

	char thread_key[MAX_KEY_SIZE]; /* "Foreign key" to thread object. */
	unsigned char _null_term_hax_1;

	char board[MAX_BOARD_NAME_SIZE];
	unsigned char _null_term_hax_2;

	char *body_content;
	size_t body_content_len;

	vector *replied_to_keys; /* keys that this post replied to. */
} __attribute__((__packed__)) post;

void create_post_key(const char board[static MAX_BOARD_NAME_SIZE], const char *post_id,
	char outbuf[static MAX_KEY_SIZE]);
char *serialize_post(const post *to_serialize);
thread *deserialize_post(const char *json);
