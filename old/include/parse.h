// vim: noet ts=4 sw=4
#pragma once
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "stack.h"
#include "models.h"

typedef struct thread_match {
	char board[MAX_BOARD_NAME_SIZE];
	uint64_t thread_num;
} thread_match;

typedef struct post_match {
	char board[MAX_BOARD_NAME_SIZE];
	char filename[MAX_IMAGE_FILENAME_SIZE];
	char subject[256];
	char file_ext[6];
	char post_no[64]; /* The real post_number */
	char post_date[64]; /* This is actually post date */
	char thread_number[64];
	char *body_content;
	size_t size; /* Size of the file */
	char should_download_image;
} post_match;

/* FUCK THE MUTEABLE STATE */
ol_stack *parse_catalog_json(const char *all_json, const char board[static MAX_BOARD_NAME_SIZE]);
ol_stack *parse_thread_json(const char *all_json, const thread_match *match);
