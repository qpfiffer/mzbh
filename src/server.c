// vim: noet ts=4 sw=4
#include <arpa/inet.h>
#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <netdb.h>
#include <regex.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "logging.h"
#include "server.h"
#include "grengine.h"
#include "greshunkel.h"

static int _only_webms_filter(const char *file_name) {
	size_t fname_siz = strlen(file_name);
	/* Wish I had endswith... */
	const char suffix[] = ".webm";
	int i = 0;
	for (; i < strlen(suffix); i++) {
		if (suffix[i] != file_name[fname_siz - strlen(suffix) + i])
			return 0;
	}
	return 1;
}

static int _add_files_in_dir_to_arr(greshunkel_var *loop, const char *dir, int (*filter_func)(const char *file_name)) {
	/* What the fuck, posix? */
	struct dirent dirent_thing = {0};

	/* TODO: Actually find out what board we're on. */
	DIR *dirstream = opendir(dir);
	int total = 0;
	while (1) {
		struct dirent *result = NULL;
		readdir_r(dirstream, &dirent_thing, &result);
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

	return total;
}

/* Various handlers for our routes: */
static int static_handler(const http_request *request, http_response *response) {
	/* Remove the leading slash: */
	const char *file_path = request->resource + sizeof(char);

	/* Figure out the mimetype for this resource: */
	char ending[4] = {0};
	int i, j = sizeof(ending);
	const size_t res_len = strlen(request->resource);
	for (i = res_len; i > (res_len - sizeof(ending)); i--) {
		ending[--j] = request->resource[i];
	}

	/* This is how we do mimetypes. lol. */
	if (strncasecmp(ending, "css", sizeof(ending)) == 0) {
		strncpy(response->mimetype, "text/css", sizeof(response->mimetype));
	} else if (strncasecmp(ending, "jpg", sizeof(ending)) == 0) {
		strncpy(response->mimetype, "image/jpeg", sizeof(response->mimetype));
	} else if (strncasecmp(ending, "webm", sizeof(ending)) == 0) {
		strncpy(response->mimetype, "video/webm", sizeof(response->mimetype));
	} else {
		strncpy(response->mimetype, "application/octet-stream", sizeof(response->mimetype));
	}
	return mmap_file(file_path, response);
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

	greshunkel_var *boards = gshkl_add_array(ctext, "BOARDS");
	_add_files_in_dir_to_arr(boards, webm_location(), NULL);

	char *rendered = gshkl_render(ctext, mmapd_region, original_size, &new_size);
	gshkl_free_context(ctext);

	/* Make sure the response is kept up to date: */
	response->outsize = new_size;
	response->out = (unsigned char *)rendered;

	/* Clean up the stuff we're no longer using. */
	munmap(response->out, original_size);
	free(response->extra_data);
	return 200;
}

static int board_handler(const http_request *request, http_response *response) {
	int rc = mmap_file("./templates/board.html", response);
	if (rc != 200)
		return rc;
	// 1. Render the mmap()'d file with greshunkel
	const char *mmapd_region = (char *)response->out;
	const size_t original_size = response->outsize;

	char current_board[32] = {0};
	const size_t board_len = request->matches[1].rm_eo - request->matches[1].rm_so;
	const size_t bgr = sizeof(current_board) > board_len ? board_len : sizeof(current_board);
	strncpy(current_board, request->resource + request->matches[1].rm_so, bgr);

	size_t new_size = 0;
	greshunkel_ctext *ctext = gshkl_init_context();
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

	/* Make sure the response is kept up to date: */
	response->outsize = new_size;
	response->out = (unsigned char *)rendered;

	/* Clean up the stuff we're no longer using. */
	munmap(response->out, original_size);
	free(response->extra_data);
	return 200;
}

static int favicon_handler(const http_request *request, http_response *response) {
	strncpy(response->mimetype, "image/x-icon", sizeof(response->mimetype));
	return mmap_file("./static/favicon.ico", response);
}

/* All other routes: */
const route all_routes[] = {
	{"GET", "^/favicon.ico$", 0, &favicon_handler, &mmap_cleanup},
	{"GET", "^/static/[a-zA-Z0-9/_-]*\\.[a-zA-Z]*$", 0, &static_handler, &mmap_cleanup},
	{"GET", "^/chug/([a-zA-Z]*)$", 1, &board_handler, &heap_cleanup},
	{"GET", "^/$", 0, &index_handler, &heap_cleanup},
};

static int respond(const int accept_fd) {
	char to_read[MAX_READ_LEN] = {0};
	char *actual_response = NULL;
	http_response response = {.mimetype = "text/html", 0};
	const route *matching_route = NULL;

	int rc = recv(accept_fd, to_read, MAX_READ_LEN, 0);
	if (rc <= 0) {
		log_msg(LOG_ERR, "Did not receive any information from accepted connection.");
		goto error;
	}

	http_request request = {
		.verb = {0},
		.resource = {0},
		.matches = {{0}}
	};
	rc = parse_request(to_read, &request);
	if (rc != 0) {
		log_msg(LOG_ERR, "Could not parse request.");
		goto error;
	}

	/* Find our matching route: */
	int i;
	for (i = 0; i < (sizeof(all_routes)/sizeof(all_routes[0])); i++) {
		const route *cur_route = &all_routes[i];
		if (strcmp(cur_route->verb, request.verb) != 0)
			continue;

		assert(cur_route->expected_matches < MAX_MATCHES);
		regex_t regex;
		int reti = regcomp(&regex, cur_route->route_match, REG_EXTENDED);
		if (reti != 0) {
			char errbuf[128];
			regerror(reti, &regex, errbuf, sizeof(errbuf));
			log_msg(LOG_ERR, "%s", errbuf);
			assert(reti == 0);
		}

		if (request.matches > 0)
			reti = regexec(&regex, request.resource, cur_route->expected_matches + 1, request.matches, 0);
		else
			reti = regexec(&regex, request.resource, 0, NULL, 0);
		regfree(&regex);
		if (reti == 0) {
			matching_route = &all_routes[i];
			break;
		}
	}

	/* If we didn't find one just use the 404 route: */
	if (matching_route == NULL)
		matching_route = &r_404_route;

	/* Run the handler through with the data we have: */
	const int response_code = matching_route->handler(&request, &response);
	assert(response.outsize > 0);
	assert(response.out != NULL);

	log_msg(LOG_FUN, "\"%s %s\" %i %i", request.verb, request.resource,
			response_code, response.outsize);

	/* Figure out what header we need to use: */
	const code_to_message *matched_response = NULL;
	const code_to_message *response_headers = get_response_headers();
	const unsigned int num_elements = get_response_headers_num_elements();
	for (i = 0; i < num_elements; i++) {
		code_to_message current_response = response_headers[i];
		if (current_response.code == response_code) {
			matched_response = &response_headers[i];
			break;
		}
	}
	/* Blow up if we don't have that code. We should have them all at
	 * compile time. */
	assert(matched_response != NULL);

	/* Embed the handler's text into the header: */
	const size_t integer_length = INT_LEN(response.outsize);
	const size_t header_size = strlen(response.mimetype) + strlen(matched_response->message) + integer_length - strlen("%s") - strlen("%zu");
	const size_t actual_response_siz = response.outsize + header_size;
	actual_response = malloc(actual_response_siz + 1);
	memset(actual_response, '\0', actual_response_siz + 1);
	/* snprintf the header because it's just a string: */
	snprintf(actual_response, actual_response_siz, matched_response->message, response.mimetype, response.outsize);
	/* memcpy the rest because it could be anything: */
	memcpy(actual_response + header_size, response.out, response.outsize);

	/* Send that shit over the wire: */
	const size_t bytes_siz = actual_response_siz;
	rc = send(accept_fd, actual_response, bytes_siz, 0);
	if (rc <= 0) {
		log_msg(LOG_ERR, "Could not send response.");
		goto error;
	}
	matching_route->cleanup(&response);
	free(actual_response);

	return 0;

error:
	if (matching_route != NULL)
		matching_route->cleanup(&response);
	free(actual_response);
	return -1;
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

	struct sockaddr_in hints = {0};
	hints.sin_family		 = AF_INET;
	hints.sin_port			 = htons(8080);
	hints.sin_addr.s_addr	 = htonl(INADDR_LOOPBACK);

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

	struct sockaddr_storage their_addr = {0};
	socklen_t sin_size = sizeof(their_addr);
	while(1) {
		int new_fd = accept(main_sock_fd, (struct sockaddr *)&their_addr, &sin_size);

		if (new_fd == -1) {
			log_msg(LOG_ERR, "Could not accept new connection.");
			goto error;
		} else {
			pid_t child = fork();
			if (child == 0) {
				respond(new_fd);
				close(new_fd);
				exit(0);
			}
		}
	}

	close(main_sock_fd);
	return 0;

error:
	perror("Socket error");
	close(main_sock_fd);
	return rc;
}

