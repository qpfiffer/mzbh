// vim: noet ts=4 sw=4
#include <string.h>
#include <sys/stat.h>
#include "utils.h"

const char WEBMS_DIR_DEFAULT[] = "./webms";
const char *WEBMS_DIR = NULL;

const char *webm_location() {
	if (!WEBMS_DIR) {
		char *env_var = getenv("WEBMS_DIR");
		if (!env_var) {
			WEBMS_DIR = WEBMS_DIR_DEFAULT;
		} else {
			WEBMS_DIR = env_var;
		}
	}

	return WEBMS_DIR;
}

int endswith(const char *string, const char *suffix) {
	size_t string_siz = strlen(string);
	size_t suffix_siz = strlen(suffix);

	if (string_siz < suffix_siz)
		return 0;

	int i = 0;
	for (; i < suffix_siz; i++) {
		if (suffix[i] != string[string_siz - suffix_siz + i])
			return 0;
	}
	return 1;
}

/* Pulled from here: http://stackoverflow.com/a/25705264 */
char *strnstr(const char *haystack, const char *needle, size_t len) {
	int i;
	size_t needle_len;

	/* segfault here if needle is not NULL terminated */
	if (0 == (needle_len = strlen(needle)))
		return (char *)haystack;

	/* Limit the search if haystack is shorter than 'len' */
	len = strnlen(haystack, len);

	for (i=0; i<(int)(len-needle_len); i++)
	{
		if ((haystack[0] == needle[0]) && (0 == strncmp(haystack, needle, needle_len)))
			return (char *)haystack;

		haystack++;
	}
	return NULL;
}
