// vim: noet ts=4 sw=4
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "parse.h"
#include "parson.h"
#include "logging.h"

ol_stack *parse_catalog_json(const char *all_json, const char board[BOARD_STR_LEN]) {
	JSON_Value *catalog = json_parse_string(all_json);

	if (json_value_get_type(catalog) != JSONArray)
		log_msg(LOG_WARN, "Well, the root isn't a JSONArray.");

	ol_stack *matches = NULL;
	matches = calloc(1, sizeof(ol_stack));

	JSON_Array *all_objects = json_value_get_array(catalog);

	int i;
	const int page_count = json_array_get_count(all_objects);
	for (i = 0; i < page_count; i++) {
		JSON_Object *obj = json_array_get_object(all_objects, i);
		log_msg(LOG_INFO, "Checking Page: %lu/%i", (long)json_object_get_number(obj, "page"), page_count);

		JSON_Array *threads = json_object_get_array(obj, "threads");
		int j;
		for (j = 0; j < json_array_get_count(threads); j++) {
			JSON_Object *thread = json_array_get_object(threads, j);
			const int thread_num = json_object_get_number(thread, "no");
			const char *file_ext = json_object_get_string(thread, "ext");
			const char *post = json_object_get_string(thread, "com");

			if ((file_ext != NULL && strstr(file_ext, "webm")) ||
				(post != NULL && strcasestr(post, "webm")) ||
				(post != NULL && strcasestr(post, "gif"))) {
				log_msg(LOG_INFO, "Thread %i may have some webm. Ext: %s", thread_num, file_ext);

				thread_match _match = {
					.thread_num = thread_num
				};

				thread_match *match = malloc(sizeof(thread_match));
				memcpy(match, &_match, sizeof(_match));
				strncpy(match->board, board, BOARD_STR_LEN);

				spush(&matches, match);
			}
		}
	}

	json_value_free(catalog);
	return matches;
}

ol_stack *parse_thread_json(const char *all_json, const thread_match *match) {
	JSON_Value *thread_raw = json_parse_string(all_json);
	JSON_Object *root = json_value_get_object(thread_raw);

	ol_stack *matches = NULL;
	matches = malloc(sizeof(ol_stack));
	memset(matches, 0, sizeof(ol_stack));

	JSON_Array *posts = json_object_get_array(root, "posts");
	int i;
	for (i = 0; i < json_array_get_count(posts); i++) {
		JSON_Object *post = json_array_get_object(posts, i);
		const char *file_ext = json_object_get_string(post, "ext");
		const char *filename = json_object_get_string(post, "filename");
		const long _tim = (long)json_object_get_number(post, "tim");

		if (file_ext == NULL)
			continue;

		if (strstr(file_ext, "webm")) {
			log_msg(LOG_INFO, "Hit: %s%s.", filename, file_ext);

			char tim[32] = {0};
			snprintf(tim, sizeof(tim), "%lu", _tim);

			post_match *t_match = malloc(sizeof(post_match));
			strncpy(t_match->tim, tim, sizeof(t_match->tim));
			strncpy(t_match->filename, filename, sizeof(t_match->filename));
			strncpy(t_match->file_ext, file_ext, sizeof(t_match->file_ext));
			strncpy(t_match->board, match->board, sizeof(t_match->board));

			spush(&matches, t_match);
		}
	}

	return matches;
}

