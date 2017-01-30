// vim: noet ts=4 sw=4
#ifdef __clang__
	#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#endif
#include <arpa/inet.h>
#include <assert.h>
#include <dirent.h>
#include <errno.h>
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

#include <38-moths/38-moths.h>

#include "db.h"
#include "http.h"
#include "parse.h"
#include "models.h"
#include "server.h"

#define RESULTS_PER_PAGE 80
#define OFFSET_FOR_PAGE(x) x * RESULTS_PER_PAGE

static char *pretty_date(const char *argument) {
	const time_t tim = strtol(argument, NULL, 10);

	if ((tim == LONG_MIN || tim == LONG_MAX) && errno == ERANGE)
		return strdup(argument);

	const time_t non_milli_tim = tim / 1000;

	struct tm converted;
	gmtime_r(&non_milli_tim, &converted);

	char *buf = calloc(1, sizeof(char) * 25);
    strftime(buf, 25, "%F(%a)%T", &converted);

	return buf;
}

static char *thumbnail_for_image(const char *argument) {
	const size_t arg_len = strlen(argument);
	const size_t stop_at = arg_len - strlen("webm");

	const char prefix[] = "t/thumb_";
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
	/* Check to make sure the directory actually exists. */
	struct stat dir_st = {0};
	if (stat(dir, &dir_st) == -1)
		return 0;

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
		vector_free(webm_vec);
		return 0;
	}

	qsort(webm_vec->items, webm_vec->count, webm_vec->item_size, &compare_dates);
	uint64_t i;
	for (i = 0; i < webm_vec->count; i++) {
		int8_t can_add = 0;
		if (!limit && !offset)
			can_add = 1;
		else if (i >= offset && i < (offset + limit))
			can_add = 1;

		if (can_add) {
			const struct file_and_time *x = vector_get(webm_vec, i);
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
		vector_free(alphabetical_vec);
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
	gshkl_add_int(ctext, "post_count", post_count());

	greshunkel_var boards = gshkl_add_array(ctext, "BOARDS");
	_add_files_in_dir_to_arr(&boards, webm_location());
	return render_file(ctext, "./templates/index.html", response);
}

int webm_handler(const http_request *request, http_response *response) {
	char current_board[MAX_BOARD_NAME_SIZE] = {0};
	get_current_board(current_board, request);

	greshunkel_ctext *ctext = gshkl_init_context();
	gshkl_add_string(ctext, "current_board", current_board);
	gshkl_add_filter(ctext, "pretty_date", &pretty_date, &filter_cleanup);

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

	char webm_key[MAX_KEY_SIZE] = {0};
	hash_file(full_path, image_hash);
	webm *_webm = get_image(image_hash, webm_key);

	char alias_key[MAX_KEY_SIZE] = {0};
	webm_alias *_alias = get_aliased_image(full_path, alias_key);

	/* This code is fucking terrible. */
	int found = 0;
	if (_webm) {
		found = 1;
		if (_alias) {
			/* This shouldn't happen, but whatever. */
			free(_alias);
		}
	} else if (_alias) {
		found = 1;
		free(_webm);
		_webm = (webm *)_alias;
	}

	if (!found) {
		gshkl_add_int(ctext, "image_date", -1);
		gshkl_add_string(ctext, "post_content", NULL);
		gshkl_add_string(ctext, "post_id", NULL);
		gshkl_add_string(ctext, "thread_id", NULL);
	} else {
		post *_post = get_post(_webm->post);
		if (_post) {
			gshkl_add_string(ctext, "thread_id", _post->thread_key);
			gshkl_add_string(ctext, "post_id", _post->post_id);
			if (_post->body_content) {
				gshkl_add_string(ctext, "post_content", _post->body_content);
				free(_post->body_content);
			} else {
				gshkl_add_string(ctext, "post_content", NULL);
			}
			vector_free(_post->replied_to_keys);
		} else {
			gshkl_add_string(ctext, "post_content", NULL);
			gshkl_add_string(ctext, "post_id", NULL);
			gshkl_add_string(ctext, "thread_id", NULL);
		}
		free(_post);
		time_t earliest_date = _webm->created_at;

		/* Add known aliases from DB. We fetch every alias from the M2M,
		 * and then fetch that key. Or try to, anyway. */
		webm_to_alias *w2a = get_webm_to_alias(image_hash);
		if (w2a != NULL && w2a->aliases->count > 0) {
			unsigned int i;
			/* TODO: Use bulk_unjar here. */
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

	free(_webm);
	free(full_path);
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

	/* Check to make sure the directory actually exists. */
	struct stat dir_st = {0};
	if (stat(images_dir, &dir_st) == -1)
		return 404;

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

static unsigned int _add_sorted_by_aliases(greshunkel_var *images) {
	char p[MAX_KEY_SIZE] = WEBMTOALIAS_NMSPC;
	db_key_match *key_matches = fetch_matches_from_db(&oleg_conn, p);
	db_match *matches = fetch_bulk_from_db(&oleg_conn, key_matches, 1);

	unsigned int total = 0;
	db_match *current = matches;
	while (current) {
		db_match *next = current->next;

		webm_to_alias *dsrlzd = deserialize_webm_to_alias((char *)current->data);
		free((unsigned char *)current->data);
		free(current);

		unsigned int i = 0;
		for (i = 0; i < dsrlzd->aliases->count; i++) {
			gshkl_add_string_to_loop(images, vector_get(dsrlzd->aliases, i));
		}

		vector_free(dsrlzd->aliases);
		free(dsrlzd);
		total++;

		current = next;
	}

	return 0;
}

static db_key_match *create_match_keys_from_vector(const vector *vec) {
	db_key_match *cur = NULL;
	unsigned int i;
	for (i = 0; i < vec->count; i++) {
		db_key_match _stack = {
			.key = {0},
			.next = cur
		};

		const char *_key = vector_get(vec, i);
		size_t larger = strlen(_key) > MAX_KEY_SIZE ? MAX_KEY_SIZE : strlen(_key);
		strncpy((char *)_stack.key, _key, larger);

		db_key_match *new = calloc(1, sizeof(db_key_match));
		memcpy(new, &_stack, sizeof(db_key_match));

		cur = new;
	}

	return cur;
}

int by_thread_handler(const http_request *request, http_response *response) {
	char thread_id[256] = {0};
	strncpy(thread_id, request->resource + request->matches[1].rm_so, sizeof(thread_id));

	if (thread_id == NULL || request->resource + request->matches[1].rm_so == 0)
		return 404;

	thread *_thread = get_thread(thread_id);
	if (_thread == NULL)
		return 404;

	greshunkel_ctext *ctext = gshkl_init_context();
	gshkl_add_string(ctext, "thread_id", thread_id);
	gshkl_add_filter(ctext, "pretty_date", pretty_date, filter_cleanup);
	gshkl_add_filter(ctext, "thumbnail_for_image", thumbnail_for_image, filter_cleanup);

	greshunkel_var posts = gshkl_add_array(ctext, "POSTS");

	db_key_match *cur = create_match_keys_from_vector(_thread->post_keys);

	/* So this is kind of fucked because we're going to be getting back both
	 * webms and webm_aliases from the DB, but we're going to deserialize them
	 * all to webm_alias objects. This will work because a webm_alias is a subset
	 * of a webm.
	 */
	vector *_webms_from_posts = vector_new(MAX_KEY_SIZE, 32);
	/* This is another vector of all of the greshunkel contexts we're going to modify
	 * to add our webm content to once we've looped through once.
	 * That probably didn't make any sense.
	 */
	vector *_post_context_objs = vector_new(sizeof(struct greshunkel_ctext), 32);

	unsigned int total = 0;
	if (cur != NULL) {
		db_match *matches = fetch_bulk_from_db(&oleg_conn, cur, 1);

		/* So here we iterate through both loops, the matches and the keys. */
		unsigned int i = 0;
		db_match *current = matches;
		while (current) {
			db_match *next = current->next;

			const char *_key = vector_get(_thread->post_keys, i);

			post *dsrlzd = deserialize_post((char *)current->data);
			free((unsigned char *)current->data);
			free(current);

			if (dsrlzd) {
				greshunkel_ctext *_post_sub = gshkl_init_context();
				gshkl_add_string(_post_sub, "date", dsrlzd->post_id);
				gshkl_add_string(_post_sub, "key", _key);
				gshkl_add_string(_post_sub, "board", dsrlzd->board);

				if (dsrlzd->body_content)
					gshkl_add_string(_post_sub, "content", dsrlzd->body_content);
				else
					gshkl_add_string(_post_sub, "content", "");

				if (dsrlzd->post_no)
					gshkl_add_string(_post_sub, "post_no", dsrlzd->post_no);
				else
					gshkl_add_string(_post_sub, "post_no", "");

				if (dsrlzd->webm_key && strlen(dsrlzd->webm_key) > 0) {
					gshkl_add_string(_post_sub, "webm_key", dsrlzd->webm_key);
					/* Add the _webm_key and context to the relevant vectors for later modification
					 * after a bulk_get.
					 */
					vector_append(_webms_from_posts, dsrlzd->webm_key, sizeof(dsrlzd->webm_key));
				} else {
					vector_append(_webms_from_posts, NULL, 0);
				}
				vector_append(_post_context_objs, _post_sub, sizeof(struct greshunkel_ctext));

				gshkl_add_sub_context_to_loop(&posts, _post_sub);

				vector_free(dsrlzd->replied_to_keys);
				free(dsrlzd->body_content);
			}

			free(dsrlzd);
			total++;
			i++;

			current = next;
		}
	}

	if (_webms_from_posts->count > 0) {
		db_key_match *_all_webm_keys = create_match_keys_from_vector(_webms_from_posts);

		db_match *matches = fetch_bulk_from_db(&oleg_conn, _all_webm_keys, 1);

		unsigned int i = 0;
		db_match *current = matches;
		while (current) {
			db_match *next = current->next;
			struct greshunkel_ctext *post_ctext = (struct greshunkel_ctext *)vector_get(_post_context_objs, i);

			if (current->data == NULL) {
				gshkl_add_string(post_ctext, "image", "");
				goto done;
			} else {

				webm_alias *dsrlzd = deserialize_alias((char *)current->data);
				free((unsigned char *)current->data);

				if (dsrlzd && dsrlzd->file_path)
					gshkl_add_string(post_ctext, "image", dsrlzd->filename);

				free(dsrlzd);
			}

done:
			free(current);
			current = next;
			i++;
		}
	}

	vector_free(_webms_from_posts);
	vector_free(_post_context_objs);
	vector_free(_thread->post_keys);
	free(_thread);

	greshunkel_var boards = gshkl_add_array(ctext, "BOARDS");
	_add_files_in_dir_to_arr(&boards, webm_location());

	gshkl_add_int(ctext, "total", total);

	return render_file(ctext, "./templates/by_thread.html", response);
}

int by_alias_handler(const http_request *request, http_response *response) {
	const unsigned int page = strtol(request->resource + request->matches[1].rm_so, NULL, 10);

	greshunkel_ctext *ctext = gshkl_init_context();
	gshkl_add_filter(ctext, "thumbnail_for_image", thumbnail_for_image, filter_cleanup);
	greshunkel_var images = gshkl_add_array(ctext, "IMAGES");
	int total = _add_sorted_by_aliases(&images);
	if (total == 0) {
		gshkl_add_string_to_loop(&images, "None");
	}

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
