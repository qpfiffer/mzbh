// vim: noet ts=4 sw=4
#pragma once
#include <time.h>
#include "common_defs.h"

typedef struct webm {
	unsigned char is_alias;
	char file_hash[HASH_ARRAY_SIZE];
	char filename[MAX_IMAGE_FILENAME_SIZE];
	char board[MAX_BOARD_NAME_SIZE];
	time_t created_at;
	size_t size;
} webm;

char *serialize_webm(const webm *to_serialize);
