// vim: noet ts=4 sw=4
#pragma once
#include <hiredis/hiredis.h>
#include "common_defs.h"

#define DB_HOST "localhost"
#define DB_PORT 6379

/* Global oleg connection for this project. */
struct db_conn {
	char host[255];
	unsigned int port;
	struct timeval timeout;
};

static const struct db_conn redis_conn = {
	.host = DB_HOST,
	.port = DB_PORT,
	.timeout = {2, 500000} /* 2.5 seconds */
};

/* Gets an aliased image from the DB. */
struct webm_alias *get_aliased_image(const char filepath[static MAX_IMAGE_FILENAME_SIZE], char out_key[static MAX_KEY_SIZE]);
struct webm *get_image(const char image_hash[static HASH_ARRAY_SIZE], char out_key[static MAX_KEY_SIZE]);
struct webm_to_alias *get_webm_to_alias(const char image_hash[static HASH_ARRAY_SIZE]);

struct webm_alias *get_aliased_image_with_key(const char key[static MAX_KEY_SIZE]);

/* Attempts to add an image to the database.
 * Returns 0 on success.
 */
int add_image_to_db(const char *file_path, const char *filename, const char board[MAX_BOARD_NAME_SIZE],
		const char post_key[MAX_KEY_SIZE], char out_webm_key[static MAX_KEY_SIZE]);

/* Attempts to add a new post to the database.
 * Returns 0 on success.
 */
struct post_match;
int add_post_to_db(const struct post_match *p_match, const char webm_key[static MAX_KEY_SIZE]);
struct post *get_post(const char key[static MAX_KEY_SIZE]);
struct thread *get_thread(const char key[static MAX_KEY_SIZE]);

/* Associates a webm with an alias.
 * Returns 1 on success.
 */
int associate_alias_with_webm(const struct webm *webm, const char alias_key[static MAX_KEY_SIZE]);
