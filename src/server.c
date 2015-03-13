// vim: noet ts=4 sw=4
#ifdef __clang__
	#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#endif
#include <arpa/inet.h>
#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <regex.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <time.h>

#include <38-moths/greshunkel.h>
#include <38-moths/grengine.h>
#include <38-moths/server.h>

#include "db.h"
#include "http.h"
#include "logging.h"
#include "parse.h"
#include "models.h"
#include "server.h"

#define RESULTS_PER_PAGE 80
#define OFFSET_FOR_PAGE(x) x * RESULTS_PER_PAGE

static char *thumbnail_for_image(const char *argument) {
	const size_t arg_len = strlen(argument);
	const size_t stop_at = arg_len - strlen("webm");

	const char prefix[] = "thumb_";
	const size_t prefix_siz = strlen(prefix);

	char *to_return = calloc(1, stop_at + strlen("jpg") + prefix_siz + 1);
	strncpy(to_return, prefix, prefix_siz);
	strncat(to_return, argument, stop_at);
	strncat(to_return, "jpg", strlen("jpg"));
	strncpy(to_return + prefix_siz + stop_at, "jpg", strlen("jpg"));
	return to_return;
}

static inline int alphabetical_cmp(const void *a, const void *b) {
	return strncmp((char *)a, (char *)b, MAX_IMAGE_FILENAME_SIZE);
}

static inline int compare_dates(const void *a, const void *b) {
	const struct file_and_time *_a = a;
	const struct file_and_time *_b = b;

	return _b->ctime - _a->ctime;
}

static int _add_webms_in_dir_by_date(greshunkel_var *loop, const char *dir,
		const unsigned int offset, const unsigned int limit) {
	size_t dirent_siz = offsetof(struct dirent, d_name) +
							  pathconf(dir, _PC_NAME_MAX) + 1;
	struct dirent *dirent_thing = calloc(1, dirent_siz);

	DIR *dirstream = opendir(dir);
	unsigned int total = 0;
	vector *webm_vec = vector_new(sizeof(struct file_and_time), 2048);

	while (1) {
		struct dirent *result = NULL;
		readdir_r(dirstream, dirent_thing, &result);
		if (!result)
			break;

		if (result->d_name[0] != '.' && endswith(result->d_name, ".webm")) {
			struct stat st = {0};
			char *full_path = get_full_path_for_file(dir, result->d_name);
			if (stat(full_path, &st) == -1) {
				log_msg(LOG_ERR, "Could not stat file: %s", result->d_name);
				free(full_path);
				continue;
			}

			struct file_and_time new = {
				.fname = {0},
				.ctime = st.st_mtime
			};
			strncpy(new.fname, result->d_name, sizeof(new.fname));
			vector_append(webm_vec, &new, sizeof(struct file_and_time));
			free(full_path);
			total++;
		}
	}
	closedir(dirstream);
	free(dirent_thing);

	if (webm_vec->count <= 0) {
		gshkl_add_string_to_loop(loop, "None");
		return 0;
	}

	qsort(webm_vec->items, webm_vec->count, webm_vec->item_size, &compare_dates);
	uint64_t i;
	for (i = 0; i < webm_vec->count; i++) {
		int8_t can_add = 0;
		const struct file_and_time *x = vector_get(webm_vec, i);
		if (!limit && !offset)
			can_add = 1;
		else if (i >= offset && i < (offset + limit))
			can_add = 1;
		if (can_add) {
			gshkl_add_string_to_loop(loop, x->fname);
		}
	}

	vector_free(webm_vec);
	return total;
}

static int _add_files_in_dir_to_arr(greshunkel_var *loop, const char *dir) {
	/* Apparently readdir_r can be stack-smashed so we do it on the heap
	 * instead.
	 */
	size_t dirent_siz = offsetof(struct dirent, d_name) +
							  pathconf(dir, _PC_NAME_MAX) + 1;
	struct dirent *dirent_thing = calloc(1, dirent_siz);
	vector *alphabetical_vec = vector_new(MAX_IMAGE_FILENAME_SIZE, 16);

	DIR *dirstream = opendir(dir);
	unsigned int total = 0;
	while (1) {
		struct dirent *result = NULL;
		readdir_r(dirstream, dirent_thing, &result);
		if (!result)
			break;

		if (result->d_name[0] != '.') {
			vector_append(alphabetical_vec, result->d_name, strnlen(result->d_name, MAX_IMAGE_FILENAME_SIZE));
			total++;
		}
	}
	closedir(dirstream);
	free(dirent_thing);

	if (alphabetical_vec->count <= 0) {
		gshkl_add_string_to_loop(loop, "None");
		return 0;
	}

	qsort(alphabetical_vec->items, alphabetical_vec->count, alphabetical_vec->item_size, &alphabetical_cmp);
	uint64_t i;
	for (i = 0; i < alphabetical_vec->count; i++) {
		const char *name = vector_get(alphabetical_vec, i);
		gshkl_add_string_to_loop(loop, name);
	}

	vector_free(alphabetical_vec);
	return total;
}

/* Various handlers for our routes: */
int static_handler(const http_request *request, http_response *response) {
	/* Remove the leading slash: */
	const char *file_path = request->resource + sizeof(char);
	return mmap_file(file_path, response);
}

static void get_current_board(char current_board[static MAX_BOARD_NAME_SIZE], const http_request *request) {
	const size_t board_len = request->matches[1].rm_eo - request->matches[1].rm_so;
	const size_t bgr = MAX_BOARD_NAME_SIZE > board_len ? board_len : MAX_BOARD_NAME_SIZE;
	strncpy(current_board, request->resource + request->matches[1].rm_so, bgr);
}

static void get_webm_from_board(char file_name_decoded[static MAX_IMAGE_FILENAME_SIZE], const http_request *request) {
	char file_name[MAX_IMAGE_FILENAME_SIZE] = {0};
	const size_t file_name_len = request->matches[2].rm_eo - request->matches[2].rm_so;
	const size_t fname_bgr = sizeof(file_name) > file_name_len ? file_name_len : sizeof(file_name);
	strncpy(file_name, request->resource + request->matches[2].rm_so, fname_bgr);

	url_decode(file_name, file_name_len, file_name_decoded);
}

int board_static_handler(const http_request *request, http_response *response) {
	const char *webm_loc = webm_location();

	/* Current board */
	char current_board[MAX_BOARD_NAME_SIZE] = {0};
	get_current_board(current_board, request);

	/* Filename after url decoding. */
	char file_name_decoded[MAX_IMAGE_FILENAME_SIZE] = {0};
	get_webm_from_board(file_name_decoded, request);

	/* Open and mmap() the file. */
	const size_t full_path_size = strlen(webm_loc) + strlen("/") +
								  strlen(current_board) + strlen("/") +
								  strlen(file_name_decoded) + 1;
	char full_path[full_path_size];
	memset(full_path, '\0', full_path_size);
	snprintf(full_path, full_path_size, "%s/%s/%s", webm_loc, current_board, file_name_decoded);

	return mmap_file(full_path, response);
}

int index_handler(const http_request *request, http_response *response) {
	UNUSED(request);
	/* Render that shit */
	greshunkel_ctext *ctext = gshkl_init_context();
	gshkl_add_int(ctext, "webm_count", webm_count());
	gshkl_add_int(ctext, "alias_count", webm_alias_count());

	greshunkel_var boards = gshkl_add_array(ctext, "BOARDS");
	_add_files_in_dir_to_arr(&boards, webm_location());
	return render_file(ctext, "./templates/index.html", response);
}

int webm_handler(const http_request *request, http_response *response) {
	char current_board[MAX_BOARD_NAME_SIZE] = {0};
	get_current_board(current_board, request);

	greshunkel_ctext *ctext = gshkl_init_context();
	gshkl_add_string(ctext, "current_board", current_board);

	/* All boards */
	greshunkel_var boards = gshkl_add_array(ctext, "BOARDS");
	_add_files_in_dir_to_arr(&boards, webm_location());

	/* Decode the url-encoded filename. */
	char file_name_decoded[MAX_IMAGE_FILENAME_SIZE] = {0};
	get_webm_from_board(file_name_decoded, request);
	gshkl_add_string(ctext, "image", file_name_decoded);

	/* Full path, needed for the image hash */
	char *full_path = get_full_path_for_webm(current_board, file_name_decoded);

	greshunkel_var aliases = gshkl_add_array(ctext, "aliases");
	char image_hash[HASH_IMAGE_STR_SIZE] = {0};
	hash_file(full_path, image_hash);
	webm *_webm = get_image(image_hash);
	if (!_webm)
		gshkl_add_int(ctext, "image_date", -1);
	else {
		time_t earliest_date = _webm->created_at;

		/* Add known aliases from DB. We fetch every alias from the M2M,
		 * and then fetch that key. Or try to, anyway. */
		webm_to_alias *w2a = get_webm_to_alias(image_hash);
		if (w2a != NULL && w2a->aliases->count > 0) {
			unsigned int i;
			for (i = 0; i < w2a->aliases->count; i++) {
				const char *alias = vector_get(w2a->aliases, i);
				webm_alias *walias = get_aliased_image_with_key(alias);
				if (walias) {
					if (walias->created_at < earliest_date)
						earliest_date = walias->created_at;
					const size_t buf_size = UINT_LEN(walias->created_at) + strlen(", ") +
						strnlen(walias->board, MAX_BOARD_NAME_SIZE) + strlen(", ") +
						strnlen(walias->filename, MAX_IMAGE_FILENAME_SIZE);
					char buf[buf_size + 1];
					buf[buf_size] = '\0';
					snprintf(buf, buf_size, "%lld, %s, %s", (long long)walias->created_at, walias->board, walias->filename);
					gshkl_add_string_to_loop(&aliases, buf);
					free(walias);
				} else {
					log_msg(LOG_WARN, "Bad alias string: %s", alias);
					log_msg(LOG_WARN, "Bad alias on: %s%s", WEBM_NMSPC, image_hash);
					gshkl_add_string_to_loop(&aliases, alias);
				}
			}
			vector_free(w2a->aliases);
			free(w2a);
		} else {
			gshkl_add_string_to_loop(&aliases, "None");
		}

		gshkl_add_int(ctext, "image_date", earliest_date);
	}

	return render_file(ctext, "./templates/webm.html", response);
}

static int _board_handler(const http_request *request, http_response *response, const unsigned int page) {
	char current_board[MAX_BOARD_NAME_SIZE] = {0};
	get_current_board(current_board, request);

	greshunkel_ctext *ctext = gshkl_init_context();
	gshkl_add_filter(ctext, "thumbnail_for_image", thumbnail_for_image, filter_cleanup);
	gshkl_add_string(ctext, "current_board", current_board);
	greshunkel_var images = gshkl_add_array(ctext, "IMAGES");

	char images_dir[256] = {0};
	snprintf(images_dir, sizeof(images_dir), "%s/%s", webm_location(), current_board);
	int total = _add_webms_in_dir_by_date(&images, images_dir,
			OFFSET_FOR_PAGE(page), RESULTS_PER_PAGE);

	greshunkel_var pages = gshkl_add_array(ctext, "PAGES");
	unsigned int i;
	const unsigned int max = total/RESULTS_PER_PAGE;
	for (i = 0; i < max; i++)
		gshkl_add_int_to_loop(&pages, i);

	if (page > 0) {
		gshkl_add_int(ctext, "prev_page", page - 1);
	} else {
		gshkl_add_string(ctext, "prev_page", "");
	}

	if (page < max) {
		gshkl_add_int(ctext, "next_page", page + 1);
	} else {
		gshkl_add_string(ctext, "next_page", "");
	}

	greshunkel_var boards = gshkl_add_array(ctext, "BOARDS");
	_add_files_in_dir_to_arr(&boards, webm_location());

	gshkl_add_int(ctext, "total", total);

	return render_file(ctext, "./templates/board.html", response);
}

int by_alias_handler(const http_request *request, http_response *response) {
	const unsigned int page = strtol(request->resource + request->matches[1].rm_so, NULL, 10);

	greshunkel_ctext *ctext = gshkl_init_context();
	gshkl_add_filter(ctext, "thumbnail_for_image", thumbnail_for_image, filter_cleanup);
	greshunkel_var images = gshkl_add_array(ctext, "IMAGES");
	gshkl_add_string_to_loop(&images, "None");

	int total = webm_count();

	greshunkel_var pages = gshkl_add_array(ctext, "PAGES");
	unsigned int i;
	const unsigned int max = total/RESULTS_PER_PAGE;
	for (i = 0; i < max; i++)
		gshkl_add_int_to_loop(&pages, i);

	if (page > 0) {
		gshkl_add_int(ctext, "prev_page", page - 1);
	} else {
		gshkl_add_string(ctext, "prev_page", "");
	}

	if (page < max) {
		gshkl_add_int(ctext, "next_page", page + 1);
	} else {
		gshkl_add_string(ctext, "next_page", "");
	}

	greshunkel_var boards = gshkl_add_array(ctext, "BOARDS");
	_add_files_in_dir_to_arr(&boards, webm_location());

	gshkl_add_int(ctext, "total", total);
	return render_file(ctext, "./templates/sorted_by_aliases.html", response);
}

int board_handler(const http_request *request, http_response *response) {
	return _board_handler(request, response, 0);
}

int paged_board_handler(const http_request *request, http_response *response) {
	const unsigned int page = strtol(request->resource + request->matches[2].rm_so, NULL, 10);
	return _board_handler(request, response, page);
}

int favicon_handler(const http_request *request, http_response *response) {
	UNUSED(request);
	return mmap_file("./static/favicon.ico", response);
}

int robots_handler(const http_request *request, http_response *response) {
	UNUSED(request);
	return mmap_file("./static/robots.txt", response);
}
