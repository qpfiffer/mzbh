// vim: noet ts=4 sw=4
#pragma once

#define MAX_READ_LEN 1024
#define VERB_SIZE 16
#define MAX_MATCHES 4

/* Structs for you! */
/* ------------------------------------------------------------------------ */

/* Used to map between status code -> header. */
typedef struct {
	const int code;
	const char *message;
} code_to_message;

/* Parsed HTTP request object. */
typedef struct {
	char verb[VERB_SIZE];
	char resource[128];
	regmatch_t matches[MAX_MATCHES];
} http_request;

/* Fill this out and return it, signed by your parents. */
typedef struct {
	unsigned char *out;
	size_t outsize;
	char mimetype[32];
	void *extra_data;
} http_response;

typedef struct route {
	char verb[VERB_SIZE];
	char route_match[256];
	size_t expected_matches;
	int (*handler)(const http_request *request, http_response *response);
	void (*cleanup)(http_response *response);
} route;


/* Default 404 handler. */
/* ------------------------------------------------------------------------ */
int r_404_handler(const http_request *request, http_response *response);
static const route r_404_route = {
	.verb = "GET",
	.route_match = "^.*$",
	.handler = (&r_404_handler),
	.cleanup = NULL
};

/* Utility functions for command handler tasks. */
/* ------------------------------------------------------------------------ */

/* mmap()'s a file into memory and fills out the extra_data param on
 * the response object with a struct st. This needs to be present
 * to be handled later by mmap_cleanup.
 */
int mmap_file(const char *file_path, http_response *response);

/* Cleanup functions used after handlers have made a bunch of bullshit: */
void heap_cleanup(http_response *response);
void mmap_cleanup(http_response *response);

/* Get the global code to message mapping. */
const code_to_message *get_response_headers();
const size_t get_response_headers_num_elements();

/* Parses a raw bytestream into an http_request object. */
int parse_request(const char to_read[MAX_READ_LEN], http_request *out);