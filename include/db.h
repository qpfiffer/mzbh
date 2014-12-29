// vim: noet ts=4 sw=4
#pragma once
#include "common_defs.h"

#define DB_HOST "localhost"
#define DB_PORT "38080"

/* Set and get functions. */
unsigned char *fetch_data_from_db(const char key[static MAX_KEY_SIZE], size_t *outdata);
int store_data_in_db(const char key[static MAX_KEY_SIZE], const unsigned char *val, const size_t vlen);

/* Attempts to add an image to the database.
 * Returns 0 on success.
 */
int add_image_to_db(const char *file_path, const char *filename, const char board[MAX_BOARD_NAME_SIZE]);
