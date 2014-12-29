// vim: noet ts=4 sw=4
#pragma once
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include "common_defs.h"

#define INT_LEN(x) floor(log10(abs(x))) + 1
#define UINT_LEN(x) floor(log10(x)) + 1

/* Doesn't actually fully decode URLs at this point. */
void url_decode(const char *src, const size_t src_siz, char *dest);

/* String helper stuff. */
int endswith(const char *string, const char *suffix);
char *strnstr(const char *haystack, const char *needle, size_t len);

/* Constant variables in global space. Woooo! */
const char *webm_location();
const char *db_location();

time_t get_file_creation_date(const char *file_path);
size_t get_file_size(const char *file_path);

void ensure_directory_for_board(const char *board);

struct post_match;
int get_non_colliding_image_filename(char fname[static MAX_IMAGE_FILENAME_SIZE], const struct post_match *p_match);
void get_thumb_filename(char thumb_filename[static MAX_IMAGE_FILENAME_SIZE], const struct post_match *p_match);

int hash_string(const unsigned char *string, const size_t siz, char outbuf[static HASH_IMAGE_STR_SIZE]);
int hash_file(const char *filepath, char outbuf[static HASH_IMAGE_STR_SIZE]);
