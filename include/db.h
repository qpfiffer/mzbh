// vim: noet ts=4 sw=4
#pragma once
#include <oleg-http/oleg-http.h>
#include "common_defs.h"

#define DB_HOST "localhost"
#define DB_PORT "38080"

/* Global oleg connection for this project. */
static const struct db_conn oleg_conn = {
	.host = DB_HOST,
	.port = DB_PORT,
	.db_name = WAIFU_NMSPC
};

/* Gets an aliased image from the DB. */
struct webm_alias *get_aliased_image(const char filepath[static MAX_IMAGE_FILENAME_SIZE]);
struct webm *get_image(const char image_hash[static HASH_ARRAY_SIZE]);
struct webm_to_alias *get_webm_to_alias(const char image_hash[static HASH_ARRAY_SIZE]);

struct webm_alias *get_aliased_image_with_key(const char key[static MAX_KEY_SIZE]);

/* Attempts to add an image to the database.
 * Returns 0 on success.
 */
int add_image_to_db(const char *file_path, const char *filename, const char board[MAX_BOARD_NAME_SIZE]);

/* Associates a webm with an alias.
 * Returns 1 on success.
 */
int associate_alias_with_webm(const struct webm *webm, const char alias_key[static MAX_KEY_SIZE]);
