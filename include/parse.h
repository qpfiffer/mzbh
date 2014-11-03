// vim: noet ts=4 sw=4
#pragma once
#include "stack.h"

#define BOARD_STR_LEN 16

typedef struct thread_match {
	char board[BOARD_STR_LEN];
	const int thread_num;
} thread_match;

typedef struct post_match {
	char board[BOARD_STR_LEN];
	char filename[32];
	char file_ext[6];
	char tim[32];
} post_match;

/* FUCK THE MUTEABLE STATE */
ol_stack *parse_catalog_json(const char *all_json, const char board[BOARD_STR_LEN]);
ol_stack *parse_thread_json(const char *all_json, const thread_match *match);
