// vim: noet ts=4 sw=4
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "parse.h"
#include "parson.h"

ol_stack *parse_catalog_json(const char *all_json, const char board) {
	JSON_Value *catalog = json_parse_string(all_json);

	if (json_value_get_type(catalog) != JSONArray)
		printf("Well, the root isn't a JSONArray.\n");

	ol_stack *matches = NULL;
	matches = calloc(1, sizeof(ol_stack));

	JSON_Array *all_objects = json_value_get_array(catalog);

	int i;
	for (i = 0; i < json_array_get_count(all_objects); i++) {
		JSON_Object *obj = json_array_get_object(all_objects, i);
		printf("Page: %lu\n", (long)json_object_get_number(obj, "page"));

		JSON_Array *threads = json_object_get_array(obj, "threads");
		int j;
		for (j = 0; j < json_array_get_count(threads); j++) {
			JSON_Object *thread = json_array_get_object(threads, j);
			const int thread_num = json_object_get_number(thread, "no");
			const char *file_ext = json_object_get_string(thread, "ext");
			const char *post = json_object_get_string(thread, "com");

			if (strstr(file_ext, "webm") || strstr(post, "webm")) {
				printf("%i probably has a webm. Ext: %s\n%s\n", thread_num, file_ext, post);

				thread_match _match = {
					.board = board,
					.thread_num = thread_num
				};

				thread_match *match = malloc(sizeof(thread_match));
				memcpy(match, &_match, sizeof(_match));

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
			printf("Score: %s%s.\n", filename, file_ext);
			/* Images: http(s)://i.4cdn.org/<board>/<tim>.<file_ext>
			 * Thumbnails: http(s)://t.4cdn.org/<board>/<tim>s.jpg
			 */

			char tim[32] = {0};
			snprintf(tim, sizeof(tim), "%lu", _tim);

			post_match _match = {
				.board = match->board,
				.filename = {0},
				.file_ext = {0},
				.tim	  = {0}
			};

			post_match *t_match = malloc(sizeof(post_match));
			memcpy(t_match, &_match, sizeof(_match));
			strncpy(t_match->tim, tim, sizeof(t_match->tim));
			strncpy(t_match->filename, filename, sizeof(t_match->filename));
			strncpy(t_match->file_ext, file_ext, sizeof(t_match->file_ext));

			spush(&matches, t_match);
		}
	}

	return matches;
}

