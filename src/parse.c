// vim: noet ts=4 sw=4
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "parse.h"
#include "parson.h"
#include "logging.h"
#include "models.h"

ol_stack *parse_catalog_json(const char *all_json, const char board[MAX_BOARD_NAME_SIZE]) {
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
		log_msg(LOG_INFO, "/%s/ - Checking Page: %lu/%i", board, (long)json_object_get_number(obj, "page"), page_count);

		JSON_Array *threads = json_object_get_array(obj, "threads");
		unsigned int j;
		for (j = 0; j < json_array_get_count(threads); j++) {
			JSON_Object *thread = json_array_get_object(threads, j);
			JSON_Array *thread_replies = json_object_get_array(thread, "last_replies");
			const int thread_num = json_object_get_number(thread, "no");
			const char *file_ext = json_object_get_string(thread, "ext");
			const char *post = json_object_get_string(thread, "com");


			int found_webm_in_reply = 0;
			unsigned int k;
			for (k = 0; k < json_array_get_count(thread_replies); k++) {
				JSON_Object *thread_reply = json_array_get_object(thread_replies, k);
				const char *file_ext_reply = json_object_get_string(thread_reply, "ext");
				if (file_ext_reply != NULL && strstr(file_ext_reply, "webm")) {
					log_msg(LOG_INFO, "/%s/ - Found webm in reply. Adding to threads to look through.", board);
					found_webm_in_reply = 1;
					break;
				}
			}

			if (found_webm_in_reply == 1||
				(file_ext != NULL && strstr(file_ext, "webm")) ||
				(post != NULL && strcasestr(post, "webm")) ||
				(post != NULL && strcasestr(post, "gif"))) {
				log_msg(LOG_INFO, "/%s/ - Thread %i may have some webm. Ext: %s", board, thread_num, file_ext);

				thread_match *match = malloc(sizeof(thread_match));
				match->thread_num = thread_num;
				strncpy(match->board, board, MAX_BOARD_NAME_SIZE - 1);

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
	unsigned int i;
	for (i = 0; i < json_array_get_count(posts); i++) {
		JSON_Object *post = json_array_get_object(posts, i);
		const char *file_ext = json_object_get_string(post, "ext");
		const char *filename = json_object_get_string(post, "filename");
		const uint64_t siz = json_object_get_number(post, "fsize");
		const uint64_t _tim = json_object_get_number(post, "tim");

		if (file_ext == NULL)
			continue;

		if (strstr(file_ext, "webm")) {
			log_msg(LOG_INFO, "/%s/ Hit: (%"PRIu64") %s%s.", match->board, _tim, filename, file_ext);

			post_match *t_match = calloc(1, sizeof(post_match));
			t_match->size = siz;

			char post_number[sizeof(t_match->post_number)] = {0};
			snprintf(post_number, sizeof(post_number), "%"PRIu64, _tim);

			strncpy(t_match->post_number, post_number, sizeof(t_match->post_number));
			strncpy(t_match->filename, filename, sizeof(t_match->filename));
			strncpy(t_match->file_ext, file_ext, sizeof(t_match->file_ext));
			strncpy(t_match->board, match->board, sizeof(t_match->board));

			spush(&matches, t_match);
		}
	}

	json_value_free(thread_raw);
	return matches;
}
