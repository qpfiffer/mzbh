// vim: noet ts=4 sw=4
#include <arpa/inet.h>
#include <assert.h>
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
#include "greshunkel.h"

#define MAX_READ_LEN 1024
#define VERB_SIZE 16

const char r_200[] =
	"HTTP/1.1 200 OK\r\n"
	"Content-Type: %s\r\n"
	"Content-Length: %zu\r\n"
	"Connection: close\r\n"
	"Server: waifu.xyz/bitch\r\n\r\n";

const char r_404[] =
	"HTTP/1.1 404 Not Found\r\n"
	"Content-Type: %s\r\n"
	"Content-Length: %zu\r\n"
	"Connection: close\r\n"
	"Server: waifu.xyz/bitch\r\n\r\n";

typedef struct {
	const int code;
	const char *message;
} code_to_message;

typedef struct {
	char verb[VERB_SIZE];
	char resource[128];
} http_request;

typedef struct {
	unsigned char *out;
	size_t outsize;
	char mimetype[32];
	void *extra_data;
} http_response;

typedef struct route {
	char verb[VERB_SIZE];
	char route_match[256];
	int (*handler)(const http_request *request, http_response *response);
	void (*cleanup)(http_response *response);
} route;

static int mmap_file(const char *file_path, http_response *response) {
	response->extra_data = calloc(1, sizeof(struct stat));

	if (stat(file_path, response->extra_data) == -1) {
		response->out = (unsigned char *)"<html><body><p>No such file.</p></body></html>";
		response->outsize= strlen("<html><body><p>No such file.</p></body></html>");
		return 404;
	}
	int fd = open(file_path, O_RDONLY);
	if (fd <= 0) {
		response->out = (unsigned char *)"<html><body><p>Could not open file.</p></body></html>";
		response->outsize= strlen("<html><body><p>could not open file.</p></body></html>");
		return 404;
	}

	const struct stat st = *(struct stat *)response->extra_data;
	response->out = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	response->outsize = st.st_size;

	if (response->out == MAP_FAILED) {
		response->out = (unsigned char *)"<html><body><p>Could not open file.</p></body></html>";
		response->outsize= strlen("<html><body><p>could not open file.</p></body></html>");
		close(fd);
		return 404;
	}
	close(fd);
	return 200;
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

static int r_404_handler(const http_request *request, http_response *response) {
	response->out = (unsigned char *)"<h1>\"Welcome to Die|</h1>";
	response->outsize = strlen("<h1>\"Welcome to Die|</h1>");
	return 404;
}

/* Cleanup functions used after handlers have made a bunch of bullshit: */
static void heap_cleanup(http_response *response) {
	free(response->out);
}

static void mmap_cleanup(http_response *response) {
	const struct stat *st = (struct stat *)response->extra_data;
	munmap(response->out, st->st_size);
	free(response->extra_data);
}

static void stack_cleanup(http_response *response) {
	/* Do nothing. */
}

/* Used to handle 404s: */
const route r_404_route = {
	.verb = "GET",
	.route_match = "^.*$",
	.handler = (&r_404_handler),
	.cleanup = (&stack_cleanup)
};

/* This is used to map between the return codes of responses to their headers: */
const code_to_message response_headers[] = {
	{200, r_200},
	{404, r_404}
};

/* All other routes: */
const route all_routes[] = {
	{"GET", "^/favicon.ico$", &favicon_handler, &mmap_cleanup},
	{"GET", "^/static/[a-zA-Z0-9/_-]*\\.[a-zA-Z]*$", &static_handler, &mmap_cleanup},
	{"GET", "^/$", &index_handler, &heap_cleanup},
};

static int parse_request(const char to_read[MAX_READ_LEN], http_request *out) {
	/* Find the verb */
	const char *verb_end = strnstr(to_read, " ", MAX_READ_LEN);
	if (verb_end == NULL)
		goto error;

	const size_t verb_size = verb_end - to_read >= sizeof(out->verb) ? sizeof(out->verb) - 1: verb_end - to_read;
	strncpy(out->verb, to_read, verb_size);

	if (strncmp(out->verb, "GET", verb_size) != 0) {
		log_msg(LOG_WARN, "Don't know verb %s.", out->verb);
		goto error;
	}

	const char *res_offset = verb_end + sizeof(char);
	const char *resource_end = strnstr(res_offset, " ", sizeof(out->resource));
	if (resource_end == NULL)
		goto error;

	const size_t resource_size = resource_end - res_offset >= sizeof(out->resource) ? sizeof(out->resource) : resource_end - res_offset;
	strncpy(out->resource, res_offset, resource_size);
	return 0;

error:
	return -1;
}

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
		.resource = {0}
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

		regex_t regex;
		int reti = regcomp(&regex, cur_route->route_match, 0);
		if (reti != 0) {
			char errbuf[128];
			regerror(reti, &regex, errbuf, sizeof(errbuf));
			log_msg(LOG_ERR, "%s", errbuf);
			assert(reti == 0);
		}

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
	for (i = 0; i < (sizeof(response_headers)/sizeof(response_headers[0])); i++) {
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

