// vim: noet ts=4 sw=4
#include <arpa/inet.h>
#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <netdb.h>
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

#include "logging.h"
#include "server.h"
#include "grengine.h"
#include "greshunkel.h"
#include "models.h"

#define NUM_THREADS 4

static int _only_webms_filter(const char *file_name) {
	return endswith(file_name, ".webm");
}

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

static int _add_files_in_dir_to_arr(greshunkel_var *loop, const char *dir, int (*filter_func)(const char *file_name)) {
	/* Apparently readdir_r can be stack-smashed so we do it on the heap
	 * instead.
	 */
	size_t dirent_siz = offsetof(struct dirent, d_name) +
							  pathconf(dir, _PC_NAME_MAX) + 1;
	struct dirent *dirent_thing = calloc(1, dirent_siz);

	DIR *dirstream = opendir(dir);
	int total = 0;
	while (1) {
		struct dirent *result = NULL;
		readdir_r(dirstream, dirent_thing, &result);
		if (!result)
			break;
		if (result->d_name[0] != '.') {
			if (filter_func != NULL) {
				if (filter_func(result->d_name)) {
					gshkl_add_string_to_loop(loop, result->d_name);
					total++;
				}
			} else {
				gshkl_add_string_to_loop(loop, result->d_name);
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

static void get_webm_from_from_board(char file_name_decoded[static MAX_IMAGE_FILENAME_SIZE], const http_request *request) {
	char file_name[MAX_IMAGE_FILENAME_SIZE] = {0};
	const size_t file_name_len = request->matches[2].rm_eo - request->matches[2].rm_so;
	const size_t fname_bgr = sizeof(file_name) > file_name_len ? file_name_len : sizeof(file_name);
	strncpy(file_name, request->resource + request->matches[2].rm_so, fname_bgr);

	url_decode(file_name, file_name_len, file_name_decoded);
}

static int board_static_handler(const http_request *request, http_response *response) {
	const char *webm_loc = webm_location();

	char current_board[MAX_BOARD_NAME_SIZE] = {0};
	get_current_board(current_board, request);

	char file_name_decoded[MAX_IMAGE_FILENAME_SIZE] = {0};
	get_webm_from_from_board(file_name_decoded, request);

	const size_t full_path_size = strlen(webm_loc) + strlen("/") +
								  strlen(current_board) + strlen("/") +
								  strlen(file_name_decoded) + 1;
	char full_path[full_path_size];
	memset(full_path, '\0', full_path_size);
	snprintf(full_path, full_path_size, "%s/%s/%s", webm_loc, current_board, file_name_decoded);

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
	//gshkl_add_int(ctext, "webm_count", webm_count());
	//gshkl_add_int(ctext, "alias_count", webm_alias_count());
	gshkl_add_int(ctext, "webm_count", -1);
	gshkl_add_int(ctext, "alias_count", -1);

	greshunkel_var *boards = gshkl_add_array(ctext, "BOARDS");
	_add_files_in_dir_to_arr(boards, webm_location(), NULL);

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

	greshunkel_var *boards = gshkl_add_array(ctext, "BOARDS");
	_add_files_in_dir_to_arr(boards, webm_location(), NULL);

	char file_name_decoded[MAX_IMAGE_FILENAME_SIZE] = {0};
	get_webm_from_from_board(file_name_decoded, request);
	gshkl_add_string(ctext, "image", file_name_decoded);

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
	int total = _add_files_in_dir_to_arr(images, images_dir, &_only_webms_filter);

	greshunkel_var *boards = gshkl_add_array(ctext, "BOARDS");
	_add_files_in_dir_to_arr(boards, webm_location(), NULL);

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

static int favicon_handler(const http_request *request, http_response *response) {
	strncpy(response->mimetype, "image/x-icon", sizeof(response->mimetype));
	return mmap_file("./static/favicon.ico", response);
}

/* All other routes: */
static const route all_routes[] = {
	{"GET", "^/favicon.ico$", 0, &favicon_handler, &mmap_cleanup},
	{"GET", "^/static/[a-zA-Z0-9/_-]*\\.[a-zA-Z]*$", 0, &static_handler, &mmap_cleanup},
	{"GET", "^/chug/([a-zA-Z]*)$", 1, &board_handler, &heap_cleanup},
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

int http_serve(int main_sock_fd) {
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

	/* Our acceptor pool: */
	pthread_t workers[NUM_THREADS];

	int i;
	for (i = 0; i < NUM_THREADS; i++) {
		if (pthread_create(&workers[i], NULL, acceptor, &main_sock_fd) != 0) {
			goto error;
		}
		log_msg(LOG_INFO, "Thread %i started.", i);
	}

	for (i = 0; i < NUM_THREADS; i++) {
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

