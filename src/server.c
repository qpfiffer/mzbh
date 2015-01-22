// vim: noet ts=4 sw=4
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

#include "db.h"
#include "http.h"
#include "logging.h"
#include "parse.h"
#include "server.h"
#include "grengine.h"
#include "greshunkel.h"
#include "models.h"

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

	qsort(webm_vec->items, webm_vec->count, webm_vec->item_size, &compare_dates);
	int64_t i;
	for (i = webm_vec->count - 1; i >= 0; i--) {
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

static int _add_files_in_dir_to_arr(greshunkel_var *loop, const char *dir,
		const unsigned int offset, const unsigned int limit,
		int (*filter_func)(const char *file_name)) {
	/* Apparently readdir_r can be stack-smashed so we do it on the heap
	 * instead.
	 */
	size_t dirent_siz = offsetof(struct dirent, d_name) +
							  pathconf(dir, _PC_NAME_MAX) + 1;
	struct dirent *dirent_thing = calloc(1, dirent_siz);

	DIR *dirstream = opendir(dir);
	unsigned int total = 0;
	while (1) {
		struct dirent *result = NULL;
		readdir_r(dirstream, dirent_thing, &result);
		if (!result)
			break;

		/* Wow there is some dumb fucking logic in this function. */
		if (result->d_name[0] != '.') {
			int success = 0;
			if (filter_func != NULL) {
				if (filter_func(result->d_name)) {
					success = 1;
				}
			} else {
				success = 1;
			}

			if (success) {
				if (!offset && !limit) {
					gshkl_add_string_to_loop(loop, result->d_name);
				} else {
					if (total >= offset && total < (offset + limit)) {
						gshkl_add_string_to_loop(loop, result->d_name);
					}
				}
				total++;
			}
		}
	}
	closedir(dirstream);
	free(dirent_thing);

	return total;
}

/* Various handlers for our routes: */
static int static_handler(const http_request *request, http_response *response) {
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

static int board_static_handler(const http_request *request, http_response *response) {
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

	char *range_header_value = get_header_value(request->full_header, strlen(request->full_header), "Range");
	if (range_header_value) {
		range_header range = parse_range_header(range_header_value);
		free(range_header_value);

		log_msg(LOG_INFO, "Range header parsed: Limit: %zu Offset: %zu", range.limit, range.offset);
		const size_t *c_limit = range.limit == 0 ? NULL : &range.limit;
		const size_t *c_offset = range.offset == 0 ? NULL : &range.offset;

		return mmap_file_ol(full_path, response, c_offset, c_limit);

	}

	return mmap_file(full_path, response);
}

static int index_handler(const http_request *request, http_response *response) {
	int rc = mmap_file("./templates/index.html", response);
	if (rc != 200)
		return rc;
	// 1. Render the mmap()'d file with greshunkel
	const char *mmapd_region = (char *)response->out;
	const size_t original_size = response->outsize;

	/* Render that shit */
	size_t new_size = 0;
	greshunkel_ctext *ctext = gshkl_init_context();
	gshkl_add_int(ctext, "webm_count", webm_count());
	gshkl_add_int(ctext, "alias_count", webm_alias_count());

	greshunkel_var *boards = gshkl_add_array(ctext, "BOARDS");
	_add_files_in_dir_to_arr(boards, webm_location(), 0, 0, NULL);

	char *rendered = gshkl_render(ctext, mmapd_region, original_size, &new_size);
	gshkl_free_context(ctext);

	/* Clean up the stuff we're no longer using. */
	munmap(response->out, original_size);
	free(response->extra_data);

	/* Make sure the response is kept up to date: */
	response->outsize = new_size;
	response->out = (unsigned char *)rendered;
	return 200;
}

static int webm_handler(const http_request *request, http_response *response) {
	int rc = mmap_file("./templates/webm.html", response);
	if (rc != 200)
		return rc;
	// 1. Render the mmap()'d file with greshunkel
	const char *mmapd_region = (char *)response->out;
	const size_t original_size = response->outsize;

	char current_board[MAX_BOARD_NAME_SIZE] = {0};
	get_current_board(current_board, request);

	size_t new_size = 0;
	greshunkel_ctext *ctext = gshkl_init_context();
	gshkl_add_string(ctext, "current_board", current_board);

	/* All boards */
	greshunkel_var *boards = gshkl_add_array(ctext, "BOARDS");
	_add_files_in_dir_to_arr(boards, webm_location(), 0, 0, NULL);

	/* Decode the url-encoded filename. */
	char file_name_decoded[MAX_IMAGE_FILENAME_SIZE] = {0};
	get_webm_from_board(file_name_decoded, request);
	gshkl_add_string(ctext, "image", file_name_decoded);

	/* Full path, needed for the image hash */
	char *full_path = get_full_path_for_webm(current_board, file_name_decoded);

	greshunkel_var *aliases = gshkl_add_array(ctext, "aliases");
	char image_hash[HASH_IMAGE_STR_SIZE] = {0};
	hash_file(full_path, image_hash);
	webm *_webm = get_image(image_hash);
	if (!_webm)
		gshkl_add_int(ctext, "image_date", -1);
	else {
		gshkl_add_int(ctext, "image_date", _webm->created_at);

		/* Add known aliases from DB. We fetch every alias from the M2M,
		 * and then fetch that key. Or try to, anyway. */
		webm_to_alias *w2a = get_webm_to_alias(image_hash);
		if (w2a != NULL && w2a->aliases->count > 0) {
			int i;
			for (i = 0; i < w2a->aliases->count; i++) {
				const char *alias = vector_get(w2a->aliases, i);
				webm_alias *walias = get_aliased_image_with_key(alias);
				if (walias) {
					gshkl_add_string_to_loop(aliases, walias->filename);
					free(walias);
				} else {
					log_msg(LOG_WARN, "Bad alias string: %s", alias);
					log_msg(LOG_WARN, "Bad alias on: %s%s", WEBM_NMSPC, image_hash);
					gshkl_add_string_to_loop(aliases, alias);
				}
			}
			vector_free(w2a->aliases);
			free(w2a);
		} else {
			gshkl_add_string_to_loop(aliases, "None");
		}
	}

	char *rendered = gshkl_render(ctext, mmapd_region, original_size, &new_size);
	gshkl_free_context(ctext);

	/* Clean up the stuff we're no longer using. */
	munmap(response->out, original_size);
	free(response->extra_data);
	free(full_path);

	/* Make sure the response is kept up to date: */
	response->outsize = new_size;
	response->out = (unsigned char *)rendered;
	return 200;
}

static int _board_handler(const http_request *request, http_response *response, const unsigned int page) {
	int rc = mmap_file("./templates/board.html", response);
	if (rc != 200)
		return rc;
	// 1. Render the mmap()'d file with greshunkel
	const char *mmapd_region = (char *)response->out;
	const size_t original_size = response->outsize;

	char current_board[MAX_BOARD_NAME_SIZE] = {0};
	get_current_board(current_board, request);

	size_t new_size = 0;
	greshunkel_ctext *ctext = gshkl_init_context();
	gshkl_add_filter(ctext, "thumbnail_for_image", thumbnail_for_image, filter_cleanup);
	gshkl_add_string(ctext, "current_board", current_board);
	greshunkel_var *images = gshkl_add_array(ctext, "IMAGES");

	char images_dir[256] = {0};
	snprintf(images_dir, sizeof(images_dir), "%s/%s", webm_location(), current_board);
	int total = _add_webms_in_dir_by_date(images, images_dir,
			OFFSET_FOR_PAGE(page), RESULTS_PER_PAGE);

	greshunkel_var *pages = gshkl_add_array(ctext, "PAGES");
	int i;
	const unsigned int max = total/RESULTS_PER_PAGE;
	for (i = max; i >= 0; i--)
		gshkl_add_int_to_loop(pages, i);

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

	greshunkel_var *boards = gshkl_add_array(ctext, "BOARDS");
	_add_files_in_dir_to_arr(boards, webm_location(), 0, 0, NULL);

	gshkl_add_int(ctext, "total", total);
	char *rendered = gshkl_render(ctext, mmapd_region, original_size, &new_size);
	gshkl_free_context(ctext);

	/* Clean up the stuff we're no longer using. */
	munmap(response->out, original_size);
	free(response->extra_data);

	/* Make sure the response is kept up to date: */
	response->outsize = new_size;
	response->out = (unsigned char *)rendered;
	return 200;
}

static int board_handler(const http_request *request, http_response *response) {
	return _board_handler(request, response, 0);
}

static int paged_board_handler(const http_request *request, http_response *response) {
	const unsigned int page = strtol(request->resource + request->matches[2].rm_so, NULL, 10);
	return _board_handler(request, response, page);
}

static int favicon_handler(const http_request *request, http_response *response) {
	strncpy(response->mimetype, "image/x-icon", sizeof(response->mimetype));
	return mmap_file("./static/favicon.ico", response);
}

static int robots_handler(const http_request *request, http_response *response) {
	return mmap_file("./static/robots.txt", response);
}

/* All other routes: */
static const route all_routes[] = {
	{"GET", "^/robots.txt$", 0, &robots_handler, &mmap_cleanup},
	{"GET", "^/favicon.ico$", 0, &favicon_handler, &mmap_cleanup},
	{"GET", "^/static/[a-zA-Z0-9/_-]*\\.[a-zA-Z]*$", 0, &static_handler, &mmap_cleanup},
	{"GET", "^/chug/([a-zA-Z]*)$", 1, &board_handler, &heap_cleanup},
	{"GET", "^/chug/([a-zA-Z]*)/([0-9]*)$", 2, &paged_board_handler, &heap_cleanup},
	{"GET", "^/slurp/([a-zA-Z]*)/((.*)(.webm|.jpg))$", 2, &webm_handler, &heap_cleanup},
	{"GET", "^/chug/([a-zA-Z]*)/((.*)(.webm|.jpg))$", 2, &board_static_handler, &mmap_cleanup},
	{"GET", "^/$", 0, &index_handler, &heap_cleanup},
};

static void *acceptor(void *arg) {
	const int main_sock_fd = *(int*)arg;
	while(1) {
		struct sockaddr_storage their_addr = {0};
		socklen_t sin_size = sizeof(their_addr);

		int new_fd = accept(main_sock_fd, (struct sockaddr *)&their_addr, &sin_size);

		if (new_fd == -1) {
			log_msg(LOG_ERR, "Could not accept new connection.");
			return NULL;
		} else {
			respond(new_fd, all_routes, sizeof(all_routes)/sizeof(all_routes[0]));
			close(new_fd);
		}
	}
	return NULL;
}

int http_serve(int main_sock_fd, const int num_threads) {
	/* Our acceptor pool: */
	pthread_t workers[num_threads];

	int rc = -1;
	main_sock_fd = socket(PF_INET, SOCK_STREAM, 0);
	if (main_sock_fd <= 0) {
		log_msg(LOG_ERR, "Could not create main socket.");
		goto error;
	}

	int opt = 1;
	setsockopt(main_sock_fd, SOL_SOCKET, SO_REUSEADDR, (void*) &opt, sizeof(opt));

	const int port = 8080;
	struct sockaddr_in hints = {0};
	hints.sin_family		 = AF_INET;
	hints.sin_port			 = htons(port);
	hints.sin_addr.s_addr	 = htonl(INADDR_ANY);

	rc = bind(main_sock_fd, (struct sockaddr *)&hints, sizeof(hints));
	if (rc < 0) {
		log_msg(LOG_ERR, "Could not bind main socket.");
		goto error;
	}

	rc = listen(main_sock_fd, 0);
	if (rc < 0) {
		log_msg(LOG_ERR, "Could not listen on main socket.");
		goto error;
	}
	log_msg(LOG_FUN, "Listening on http://localhost:%i/", port);

	int i;
	for (i = 0; i < num_threads; i++) {
		if (pthread_create(&workers[i], NULL, acceptor, &main_sock_fd) != 0) {
			goto error;
		}
		log_msg(LOG_INFO, "Thread %i started.", i);
	}

	for (i = 0; i < num_threads; i++) {
		pthread_join(workers[i], NULL);
		log_msg(LOG_INFO, "Thread %i stopped.", i);
	}


	close(main_sock_fd);
	return 0;

error:
	perror("Socket error");
	close(main_sock_fd);
	return rc;
}

