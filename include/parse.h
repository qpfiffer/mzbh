// vim: noet ts=4 sw=4
#pragma once
#include "stack.h"

typedef struct thread_match {
	const char board;
	const int thread_num;
} thread_match;

/* FUCK THE MUTEABLE STATE */
ol_stack *parse_catalog_json(const char *all_json);
int parse_thread_json(const char *all_json);
