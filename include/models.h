// vim: noet ts=4 sw=4
#pragma once
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

	time_t created_at;
	size_t size;
} webm;

void create_webm_key(const char file_hash[static HASH_IMAGE_STR_SIZE], char outbuf[static MAX_KEY_SIZE]);
char *serialize_webm(const webm *to_serialize);
webm *deserialize_webm(const char *json);

/* Aliases of webms have the same file hash but different names, boards, etc. Mostly metadata. */
typedef struct webm_alias {
	char file_hash[HASH_IMAGE_STR_SIZE];
	unsigned char _null_term_hax_1;

	char filename[MAX_IMAGE_FILENAME_SIZE];
	unsigned char _null_term_hax_2;

	char board[MAX_BOARD_NAME_SIZE];
	unsigned char _null_term_hax_3;

	time_t created_at;
} webm_alias;

void create_alias_key(const char filename[static MAX_IMAGE_FILENAME_SIZE], char outbuf[static MAX_KEY_SIZE]);
char *serialize_alias(const webm_alias *to_serialize);
webm_alias *deserialize_alias(const char *json);

