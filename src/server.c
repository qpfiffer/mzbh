// vim: noet ts=4 sw=4
#include <arpa/inet.h>
#include <assert.h>
#include <netdb.h>
#include <regex.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "logging.h"
#include "server.h"

#define MAX_READ_LEN 1024
#define VERB_SIZE 16

const char r_200[] =
	"HTTP/1.1 200 OK\r\n"
	"Content-Type: text/html; charset=UTF-8\r\n"
	"Content-Length: %zu\r\n"
	"Connection: close\r\n"
	"Server: waifu.xyz/bitch\r\n\r\n"
	"%s";

const char r_404[] =
	"HTTP/1.1 404 Not Found\r\n"
	"Content-Type: text/html\r\n"
	"Content-Length: %zu\r\n"
	"Connection: close\r\n"
	"Server: waifu.xyz/bitch\r\n\r\n"
	"%s";

typedef struct {
	const int code;
	const char *message;
} code_to_message;

typedef struct http_request {
	char verb[VERB_SIZE];
	char resource[128];
} http_request;

typedef struct route {
	char verb[VERB_SIZE];
	char route_match[256];
	int (*handler)(const http_request *request, char **out, size_t *outsize);
	void (*cleanup)(char **to_clean);
} route;

/* Various handlers for our routes: */
static int static_handler(const http_request *request, char **out, size_t *outsize) {
	struct stat st = {0};
	if (stat(request->resource + sizeof(char), &st) == -1) {
		(*out) = "<html><body><p>No such file.</p></body></html>";
		(*outsize) = strlen("<html><body><p>No such file.</p></body></html>");
		return 404;
	}
	(*out) = "xxx";
	(*outsize) = strlen("xxx");
	return 200;
}

static int index_handler(const http_request *request, char **out, size_t *outsize) {
	return 200;
}

static int r_404_handler(const http_request *request, char **out, size_t *outsize) {
	(*out) = "<h1>\"Welcome to Die|</h1>";
	(*outsize) = strlen("<h1>\"Welcome to Die|</h1>");
	return 404;
}

/* Cleanup functions used after handlers have made a bunch of bullshit: */
//static void heap_cleanup(char **out) {
//	free(*out);
//}

static void mmap_cleanup(char **out) {
}

static void stack_cleanup(char **out) {
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
	{"GET", "^/static/[a-zA-Z0-9_-]*\\.[a-zA-Z]*$", &static_handler, &stack_cleanup},
	{"GET", "^/$", &index_handler, &mmap_cleanup},
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
	char *data = NULL;
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

	log_msg(LOG_WARN, "%s - %s", request.verb, request.resource);

	/* Find our matching route: */
	int i;
	for (i = 0; i < (sizeof(all_routes)/sizeof(all_routes[0])); i++) {
		/* TODO: Actually use regex to match the things here. */
		const route *cur_route = &all_routes[i];
		if (strcmp(cur_route->verb, request.verb) != 0)
			continue;

		regex_t regex;
		int reti = regcomp(&regex, cur_route->route_match, 0);
		assert(reti == 0);

		reti = regexec(&regex, request.resource, 0, NULL, 0);
		if (reti == 0) {
			matching_route = &all_routes[i];
			break;
		}
	}

	/* If we didn't find one just use the 404 route: */
	if (matching_route == NULL)
		matching_route = &r_404_route;

	/* Run the handler through with the data we have: */
	data = NULL;
	size_t dsize = 0;
	const int response_code = matching_route->handler(&request, &data, &dsize);

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
	const size_t integer_length = INT_LEN(dsize);
	size_t actual_response_siz = dsize + strlen(matched_response->message) + integer_length;
	actual_response = malloc(actual_response_siz + 1);
	memset(actual_response, '\0', actual_response_siz + 1);
	snprintf(actual_response, actual_response_siz, matched_response->message, dsize, data);

	/* Send that shit over the wire: */
	rc = send(accept_fd, actual_response, actual_response_siz - strlen("%zu") - strlen("%s"), 0);
	if (rc <= 0) {
		log_msg(LOG_ERR, "Could not send response.");
		goto error;
	}
	matching_route->cleanup(&data);
	free(actual_response);

	return 0;

error:
	if (matching_route != NULL)
		matching_route->cleanup(&data);
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

