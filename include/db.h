// vim: noet ts=4 sw=4
#pragma once
#include "common_defs.h"

int hash_image(const char *file_path, char outbuf[128]);

/* Opens the waifu db.
 * Returns 0 on failure, 1 on success. */
int open_db(const char *location);

/* Attempts to add an image to the database.
 * Returns 0 if the image was a duplicate, in which case an alias
 * was added.
 * Returns 1 if the image was added and it was new.
 */
int add_image_to_db(const char *file_path, const char board[MAX_BOARD_NAME_SIZE]);
