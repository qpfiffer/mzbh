// vim: noet ts=4 sw=4
#pragma once
#include "stack.h"

typedef struct thread_match {
	const char board;
	const int thread_num;
} thread_match;

typedef struct post_match {
	const char board;
	char filename[32];
	char file_ext[6];
	char tim[32];
} post_match;

/* FUCK THE MUTEABLE STATE */
ol_stack *parse_catalog_json(const char *all_json);
ol_stack *parse_thread_json(const char *all_json, const thread_match *match);
