// vim: noet ts=4 sw=4
#pragma once
#include <time.h>

#include <38-moths/vector.h>
#include <libpq-fe.h>

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

	unsigned int post_id;
	time_t created_at;
	size_t size;
} __attribute__((__packed__)) webm;

void create_webm_key(const char file_hash[static HASH_IMAGE_STR_SIZE], char outbuf[static MAX_KEY_SIZE]);
char *serialize_webm(const webm *to_serialize);
//webm *deserialize_webm(const char *json);
webm *deserialize_webm_from_tuples(const PGresult *res);
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

	unsigned int post_id;

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

	char oleg_key[MAX_KEY_SIZE];
	unsigned char _null_term_hax_2;

	unsigned int id;
	time_t created_at;
	char *subject;
} __attribute__((__packed__)) thread;

void create_thread_key(const char board[static MAX_BOARD_NAME_SIZE], const char *thread_id,
		char outbuf[static MAX_KEY_SIZE]);
char *serialize_thread(const thread *to_serialize);
thread *deserialize_thread(const char *json);

typedef struct post {
	char board[MAX_BOARD_NAME_SIZE];
	unsigned char _null_term_hax_1;

	// char webm_key[MAX_KEY_SIZE]; /* This could be either an alias or an original. */
	// unsigned char _null_term_hax_2;

	char oleg_key[MAX_KEY_SIZE];
	unsigned char _null_term_hax_2;

	unsigned int fourchan_post_id; /* post date */
	unsigned int fourchan_post_no; /* post number */

	char *body_content;
	vector *replied_to_keys; /* keys that this post replied to. */

	unsigned int id; /* DB ID */
	unsigned int thread_id; /* "Foreign key" to thread object. */
	time_t created_at;
} __attribute__((__packed__)) post;

void create_post_key(const char board[static MAX_BOARD_NAME_SIZE], const char *post_id,
	char outbuf[static MAX_KEY_SIZE]);
char *serialize_post(const post *to_serialize);
//post *deserialize_post(const char *json);
post *deserialize_post_from_tuples(const PGresult *res, const unsigned int idx);
unsigned int post_count();
