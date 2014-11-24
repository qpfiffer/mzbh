// vim: noet ts=4 sw=4
#include <string.h>
#include <sys/stat.h>
#include "utils.h"

const char WEBMS_DIR_DEFAULT[] = "./webms";
const char *WEBMS_DIR = NULL;

const char DB_LOCATION_DEFAULT[] = "./webms/waifu.db";
const char *DB_LOCATION = NULL;

const char *webm_location() {
	if (!WEBMS_DIR) {
		char *env_var = getenv("WFU_WEBMS_DIR");
		if (!env_var) {
			WEBMS_DIR = WEBMS_DIR_DEFAULT;
		} else {
			WEBMS_DIR = env_var;
		}
	}

	return WEBMS_DIR;
}

const char *db_location() {
	if (!DB_LOCATION) {
		char *env_var = getenv("WFU_DB_LOCATION");
		if (!env_var) {
			DB_LOCATION = DB_LOCATION_DEFAULT;
		} else {
			DB_LOCATION = env_var;
		}
	}

	return DB_LOCATION;
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

void url_decode(const char *src, const size_t src_siz, char *dest) {
	int srcIter = 0, destIter = 0;
	while (srcIter < src_siz) {
		if (src[srcIter] == '%' && srcIter + 2 < src_siz) {
			/* Theres definitely a better way to do this but I don't care
			 * right now. */
			if (src[srcIter + 1] == '2' && src[srcIter + 2] == '0') {
				dest[destIter] = ' ';
				srcIter += 3;
				destIter++;
			}
			if (src[srcIter + 1] == '3' && src[srcIter + 2] == 'E') {
				dest[destIter] = '>';
				srcIter += 3;
				destIter++;
			}
			if (src[srcIter + 1] == '5' && src[srcIter + 2] == 'B') {
				dest[destIter] = '[';
				srcIter += 3;
				destIter++;
			}
			if (src[srcIter + 1] == '5' && src[srcIter + 2] == 'D') {
				dest[destIter] = ']';
				srcIter += 3;
				destIter++;
			}
		}

		dest[destIter] = src[srcIter];
		destIter++;
		srcIter++;
	}
}
