// vim: noet ts=4 sw=4
#pragma once
#include <oleg-http/oleg-http.h>
#include "common_defs.h"

#define DB_PG_CONNECTION_INFO "postgresql:///waifu"

#define DB_HOST "localhost"
#define DB_PORT "38080"

/* Global oleg connection for this project. */
static const struct db_conn oleg_conn = {
	.host = DB_HOST,
	.port = DB_PORT,
	.db_name = WAIFU_NMSPC
};

/* Gets an aliased image from the DB. */
struct webm_alias *get_aliased_image(const char filepath[static MAX_IMAGE_FILENAME_SIZE], char out_key[static MAX_KEY_SIZE]);
/* Gets a regular webm from the DB. */
struct webm *get_image_by_oleg_key(const char image_hash[static HASH_ARRAY_SIZE], char out_key[static MAX_KEY_SIZE]);
/* Gets a webm2alias from the DB. */
struct webm_to_alias *get_webm_to_alias(const char image_hash[static HASH_ARRAY_SIZE]);

/* Similar to get_aliased_image(2), but by key directly. */
struct webm_alias *get_aliased_image_with_key(const char key[static MAX_KEY_SIZE]);

/* Get the number of records in a table. */
unsigned int get_record_count_in_table(const char *query_command);

/* Attempts to add an image to the database.
 * Returns 0 on success.
 */
int add_image_to_db(const char *file_path, const char *filename, const char board[MAX_BOARD_NAME_SIZE],
		const unsigned int post_id, char out_webm_key[static MAX_KEY_SIZE]);

/* Attempts to add a new post to the database.
 * Returns 0 on success.
 */
struct post_match;
int add_post_to_db(const struct post_match *p_match);
struct post *get_post(const unsigned int id);
struct thread *get_thread(const char key[static MAX_KEY_SIZE]);

/* Associates a webm with an alias.
 * Returns 1 on success.
 */
int associate_alias_with_webm(const struct webm *webm, const char alias_key[static MAX_KEY_SIZE]);
