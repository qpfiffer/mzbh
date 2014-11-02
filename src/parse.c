// vim: noet ts=4 sw=4
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "parse.h"
#include "parson.h"

ol_stack *parse_catalog_json(const char *all_json) {
	JSON_Value *catalog = json_parse_string(all_json);

	if (json_value_get_type(catalog) != JSONArray)
		printf("Well, the root isn't a JSONArray.\n");

	ol_stack *matches = NULL;
	matches = malloc(sizeof(ol_stack));
	memset(matches, 0, sizeof(ol_stack));

	JSON_Array *all_objects = json_value_get_array(catalog);

	for (int i = 0; i < json_array_get_count(all_objects); i++) {
		JSON_Object *obj = json_array_get_object(all_objects, i);
		printf("Page: %f\n", json_object_get_number(obj, "page"));

		JSON_Array *threads = json_object_get_array(obj, "threads");
		for (int j = 0; j < json_array_get_count(threads); j++) {
			JSON_Object *thread = json_array_get_object(threads, j);
			const int thread_num = json_object_get_number(thread, "no");
			const char *file_ext = json_object_get_string(thread, "ext");
			const char *post = json_object_get_string(thread, "com");

			if (strstr(file_ext, "webm") || strstr(post, "webm")) {
				printf("%i probably has a webm. Ext: %s\n%s\n", thread_num, file_ext, post);

				thread_match _match = {
					.board = 'b',
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

int parse_thread_json(const char *all_json) {
	JSON_Value *thread_raw = json_parse_string(all_json);
	JSON_Object *root = json_value_get_object(thread_raw);

	JSON_Array *posts = json_object_get_array(root, "posts");
	for (int i = 0; i < json_array_get_count(posts); i++) {
		JSON_Object *post = json_array_get_object(posts, i);
		const char *file_ext = json_object_get_string(post, "ext");
		const double tim = json_object_get_number(post, "tim");

		if (file_ext == NULL)
			continue;

		if (strstr(file_ext, "webm")) {
			printf("Score: %lu%s.\n", (long)tim, file_ext);
			/* Images: http(s)://i.4cdn.org/<board>/<tim>.ext
			 * Thumbnails: http(s)://t.4cdn.org/<board>/<tim>s.jpg
			 */
		}
	}

	return 0;
}
