// vim: noet ts=4 sw=4
#pragma once
#include "stack.h"

typedef struct thread_match {
	const char board;
	const int thread_num;
} thread_match;

/* Lookit this, all 32-bytes n' shit */
typedef struct post_match {
	const char board;
	char filename[26];
	char file_ext[5];
} post_match;

/* FUCK THE MUTEABLE STATE */
ol_stack *parse_catalog_json(const char *all_json);
ol_stack *parse_thread_json(const char *all_json, const thread_match *match);
/* Returns a pointer to the first character of the image: */
const char *parse_image_from_http(const char *raw_http_resp);
