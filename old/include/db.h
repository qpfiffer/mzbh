// vim: noet ts=4 sw=4
#pragma once
#include <libpq-fe.h>
#include <stdint.h>
#include "common_defs.h"

#define DB_PG_CONNECTION_INFO "postgresql:///mzbh"

/* Gets an aliased image from the DB. */
struct webm_alias *get_aliased_image_by_oleg_key(const char filepath[static MAX_IMAGE_FILENAME_SIZE], char out_key[static MAX_KEY_SIZE]);
/* Gets a regular webm from the DB. */
struct webm *get_image_by_oleg_key(const char image_hash[static HASH_ARRAY_SIZE], char out_key[static MAX_KEY_SIZE]);
PGresult *get_aliases_by_webm_id(const unsigned int id);
PGresult *get_images_by_popularity(const unsigned int offset, const unsigned int limit);
/* Similar to get_aliased_image(2), but by key directly. */
struct webm_alias *get_aliased_image_with_key(const char key[static MAX_KEY_SIZE]);

/* Get the number of records in a table. */
unsigned int get_record_count_in_table(const char *query_command);

/* Attempts to add an image to the database.
 * Returns 0 on success.
 */
int add_image_to_db(const char *file_path, const char *filename, const char board[MAX_BOARD_NAME_SIZE],
		const unsigned int post_id);

/* Attempts to add a new post to the database.
 * Returns 0 on success.
 */
struct post_match;
unsigned int add_post_to_db(const struct post_match *p_match);
struct post *get_post(const unsigned int id);
PGresult *get_posts_by_thread_id(const unsigned int id);
struct thread *get_thread_by_id(const unsigned int thread_id);

/* Associates a webm with an alias.
 * Returns 1 on success.
 */
int associate_alias_with_webm(const struct webm *webm, const char alias_key[static MAX_KEY_SIZE]);

PGresult *get_api_index_state_webms();
PGresult *get_api_index_state_aliases();
PGresult *get_api_index_state_posts();
