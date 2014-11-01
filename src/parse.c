#include <stdio.h>
#include <string.h>

#include "parse.h"
#include "parson.h"

int parse_catalog_json(const char *all_json) {
	JSON_Value *catalog = json_parse_string(all_json);

	if (json_value_get_type(catalog) != JSONArray)
		printf("Well, the root isn't a JSONArray.\n");

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
			}
		}
	}

	json_value_free(catalog);
	return 0;
}
