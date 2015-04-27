// vim: noet ts=4 sw=4
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include <38-moths/logging.h>

#include "parse.h"
#include "parson.h"
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
			const uint64_t thread_num = json_object_get_number(thread, "no");
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
		const uint64_t no = json_object_get_number(post, "no");
		const uint64_t siz = json_object_get_number(post, "fsize");
		const uint64_t _tim = json_object_get_number(post, "tim");
		const char *body_content = json_object_get_string(post, "com");

		if (file_ext == NULL)
			continue;

		post_match *p_match = calloc(1, sizeof(post_match));
		p_match->size = siz;

		/* Convert the canonical post ID to a char array. */
		char post_date[sizeof(p_match->post_date)] = {0};
		snprintf(post_date, sizeof(post_date), "%"PRIu64, _tim);

		char post_no[sizeof(p_match->post_no)] = {0};
		snprintf(post_no, sizeof(post_no), "%"PRIu64, no);

		char thread_number[sizeof(p_match->thread_number)] = {0};
		snprintf(thread_number, sizeof(thread_number), "%"PRIu64, match->thread_num);

		strncpy(p_match->post_date, post_date, sizeof(p_match->post_date));
		strncpy(p_match->post_no, post_no, sizeof(p_match->post_no));
		strncpy(p_match->thread_number, thread_number, sizeof(p_match->thread_number));
		strncpy(p_match->filename, filename, sizeof(p_match->filename));
		strncpy(p_match->file_ext, file_ext, sizeof(p_match->file_ext));
		strncpy(p_match->board, match->board, sizeof(p_match->board));

		if (body_content) {
			p_match->body_content = strdup(body_content);
		} else {
			p_match->body_content = NULL;
		}

		spush(&matches, p_match);

		/* We download the whole thread, but we only download certain files. */
		if (strstr(file_ext, "webm")) {
			log_msg(LOG_INFO, "/%s/ Hit: (%"PRIu64") %s%s.", match->board, _tim, filename, file_ext);
			p_match->should_download_image = 1;
		}
	}

	json_value_free(thread_raw);
	return matches;
}
