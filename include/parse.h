// vim: noet ts=4 sw=4
#pragma once
#include "stack.h"
#include "models.h"

/* Used for answering range queries on static files. */
typedef struct range_header {
	const size_t limit;
	const size_t offset;
} range_header;

range_header parse_range_header(const char *range_query);

typedef struct thread_match {
	char board[MAX_BOARD_NAME_SIZE];
	int thread_num;
} thread_match;

typedef struct post_match {
	char board[MAX_BOARD_NAME_SIZE];
	char filename[MAX_IMAGE_FILENAME_SIZE];
	char file_ext[6];
	char post_number[64];
	size_t size;
} post_match;

/* FUCK THE MUTEABLE STATE */
ol_stack *parse_catalog_json(const char *all_json, const char board[static MAX_BOARD_NAME_SIZE]);
ol_stack *parse_thread_json(const char *all_json, const thread_match *match);
