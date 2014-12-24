// vim: noet ts=4 sw=4
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include "logging.h"
#include "models.h"
#include "parse.h"
#include "utils.h"

const char WEBMS_DIR_DEFAULT[] = "./webms";
const char *WEBMS_DIR = NULL;

const char DB_HOST_DEFAULT[] = "127.0.0.1";
const char *DB_HOST = NULL;

const int DB_PORT_DEFAULT = 38080;
const int DB_PORT = 0;

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

void ensure_directory_for_board(const char *board) {
	/* Long enough for WEBMS_DIR, a /, the board and a NULL terminator */
	const size_t buf_siz = strlen(webm_location()) + sizeof(char) * 2 + strnlen(board, MAX_BOARD_NAME_SIZE);
	char to_create[buf_siz];
	memset(to_create, '\0', buf_siz);

	/* ./webms/b */
	snprintf(to_create, buf_siz, "%s/%s", webm_location(), board);

	struct stat st = {0};
	if (stat(to_create, &st) == -1) {
		log_msg(LOG_WARN, "Creating directory %s.", to_create);
		mkdir(to_create, 0755);
	}
}


int get_non_colliding_image_filename(char fname[MAX_IMAGE_FILENAME_SIZE], const post_match *p_match) {
	snprintf(fname, MAX_IMAGE_FILENAME_SIZE, "%s/%s/%zu_%s%.*s",
			webm_location(), p_match->board, p_match->size,
			p_match->filename, (int)sizeof(p_match->file_ext),
			p_match->file_ext);

	size_t fsize = get_file_size(fname);
	if (fsize == 0) {
		return 0;
	} else if (fsize == p_match->size) {
		log_msg(LOG_INFO, "Skipping %s.", fname);
		return 1;
	} else if (fsize != p_match->size) {
		log_msg(LOG_WARN, "Found duplicate filename for %s with incorrect size. Bad download?",
				fname);
		return 0;
	}

	return 0;
}

void get_thumb_filename(char thumb_filename[MAX_IMAGE_FILENAME_SIZE], const post_match *p_match) {
	snprintf(thumb_filename, MAX_IMAGE_FILENAME_SIZE, "%s/%s/thumb_%zu_%s.jpg",
			webm_location(), p_match->board, p_match->size, p_match->filename);
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

time_t get_file_creation_date(const char *file_path) {
	struct stat st = {0};
	if (stat(file_path, &st) == -1)
		return 0;
	return st.st_mtime;
}

size_t get_file_size(const char *file_path) {
	struct stat st = {0};
	if (stat(file_path, &st) == -1)
		return 0;
	return st.st_size;
}
