// vim: noet ts=4 sw=4
#pragma once
#include "common_defs.h"

int hash_image(const char *file_path, char outbuf[HASH_IMAGE_STR_SIZE]);

char *fetch_data_from_db(const char key[MAX_KEY_SIZE]);

/* Attempts to add an image to the database.
 * Returns 0 on success.
 */
int add_image_to_db(const char *file_path, const char board[MAX_BOARD_NAME_SIZE]);
