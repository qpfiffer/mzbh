// vim: noet ts=4 sw=4
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "http.h"
#include "parse.h"

const char WEBMS_DIR[] = "./webms";

const char FOURCHAN_API_HOST[] = "a.4cdn.org";
const char FOURCHAN_THUMBNAIL_HOST[] = "t.4cdn.org";
const char FOURCHAN_IMAGE_HOST[] = "i.4cdn.org";

const char B_API_REQUEST[] =
	"GET /b/catalog.json HTTP/1.1\r\n"
	"User-Agent: Mozilla/5.0 (Windows NT 5.1; rv:31.0) Gecko/20100101 Firefox/31.0\r\n"
	"Host: a.4cdn.org\r\n"
	"Accept: application/json\r\n\r\n";

const char THREAD_REQUEST[] =
	"GET /%c/thread/%i.json HTTP/1.1\r\n"
	"User-Agent: Mozilla/5.0 (Windows NT 5.1; rv:31.0) Gecko/20100101 Firefox/31.0\r\n"
	"Host: a.4cdn.org\r\n"
	"Accept: application/json\r\n\r\n";

const char IMAGE_REQUEST[] =
	"GET /%c/%s%s HTTP/1.1\r\n"
	"User-Agent: Mozilla/5.0 (Windows NT 5.1; rv:31.0) Gecko/20100101 Firefox/31.0\r\n"
	"Host: i.4cdn.org\r\n";

const char THUMB_REQUEST[] =
	"GET /%c/%ss.jpg HTTP/1.1\r\n"
	"User-Agent: Mozilla/5.0 (Windows NT 5.1; rv:31.0) Gecko/20100101 Firefox/31.0\r\n"
	"Host: t.4cdn.org\r\n";

static char *receive_chunked_http(const int request_fd) {
	char *raw_buf = malloc(0);
	size_t buf_size = 0;
	int times_read = 0;

	fd_set chan_fds;
	FD_ZERO(&chan_fds);
	FD_SET(request_fd, &chan_fds);
	const int maxfd = request_fd;

	printf("Receiving message...\n");
	while (1) {
		times_read++;
		int num_bytes_read = 0;

		/* Wait for data to be read. */
		struct timeval tv = {
			.tv_sec = 1,
			.tv_usec = 0
		};
		select(maxfd + 1, &chan_fds, NULL, NULL, &tv);

		int count;
		/* How many bytes should we read: */
		ioctl(request_fd, FIONREAD, &count);
		if (count <= 0)
			break;
		int old_offset = buf_size;
		buf_size += count;
		raw_buf = realloc(raw_buf, buf_size);
		/* printf("IOCTL: %i.\n", count); */

		num_bytes_read = recv(request_fd, raw_buf + old_offset, count, 0);
	}
	/* printf("Full message is %s\n.", raw_buf); */

	/* 4Chan throws us data as chunk-encoded HTTP. Rad. */
	char *header_end = strstr(raw_buf, "\r\n\r\n");
	char *cursor_pos = header_end  + (sizeof(char) * 4);

	size_t json_total = 0;
	char *json_buf = malloc(0);

	while (1) {
		/* This is where the data begins. */
		char *chunk_size_start = cursor_pos;
		char *chunk_size_end = strstr(chunk_size_start, "\r\n");
		const int chunk_size_end_oft = chunk_size_end - chunk_size_start;

		/* We cheat a little and set the first \r to a \0 so strtol will
		 * do the right thing. */
		chunk_size_start[chunk_size_end_oft] = '\0';
		const int chunk_size = strtol(chunk_size_start, NULL, 16);

		/* printf("Chunk size is %i. Thing is: %s.\n", chunk_size, chunk_size_start); */
		/* The chunk string, the \r\n after it, the chunk itself and then another \r\n: */
		cursor_pos += chunk_size + chunk_size_end_oft + 4;

		/* Copy the json into a pure buffer: */
		int old_offset = json_total;
		json_total += chunk_size;
		if (json_total >= old_offset)
			json_buf = realloc(json_buf, json_total);
		/* Copy it from after the <chunk_size>\r\n to the end of the chunk. */
		memcpy(json_buf + old_offset, chunk_size_end + 2, chunk_size);
		/* Stop reading if we am play gods: */
		if ((cursor_pos - raw_buf) > buf_size || chunk_size <= 0)
			break;
	}
	printf("The total json size is %zu.\n", json_total);
	/* printf("JSON:\n%s", json_buf); */
	free(raw_buf);

	return json_buf;
}

static int connect_to_host(const char *host) {
	struct addrinfo hints = {0};
	struct addrinfo *res = NULL;
	int request_fd;

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	getaddrinfo(host, "80", &hints, &res);

	request_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (request_fd < 0)
		goto error;

	int opt = 1;
	setsockopt(request_fd, SOL_SOCKET, SO_REUSEADDR, (void*) &opt, sizeof(opt));

	int rc = connect(request_fd, res->ai_addr, res->ai_addrlen);
	if (rc == -1)
		goto error;

	freeaddrinfo(res);
	return request_fd;

error:
	printf("ERROR.\n");
	close(request_fd);
	return -1;
}

static char *receive_http(const int request_fd) {
	size_t buf_size = sizeof(char) * 256;
	char *raw_buf = malloc(buf_size);

	fd_set chan_fds;
	FD_ZERO(&chan_fds);
	FD_SET(request_fd, &chan_fds);
	const int maxfd = request_fd;

	/* Wait for data to be read. */
	struct timeval tv = {
		.tv_sec = 1,
		.tv_usec = 0
	};
	select(maxfd + 1, &chan_fds, NULL, NULL, &tv);

	while (1) {

		int count;
		/* How many bytes should we read: */
		ioctl(request_fd, FIONREAD, &count);
		if (count <= 0)
			break;
		int old_offset = buf_size;
		buf_size += count;
		raw_buf = realloc(raw_buf, buf_size);
		/* printf("IOCTL: %i.\n", count); */

		num_bytes_read = recv(request_fd, raw_buf + old_offset, count, 0);
	}
	return NULL;
}

int download_images() {
	struct stat st = {0};
	if (stat(WEBMS_DIR, &st) == -1)
		mkdir(WEBMS_DIR, 0700);

	int request_fd = connect_to_host(FOURCHAN_API_HOST);
	if (request_fd < 0)
		goto error;

	printf("Connected to 4chan.\n");
	int rc = send(request_fd, B_API_REQUEST, strlen(B_API_REQUEST), 0);
	if (strlen(B_API_REQUEST) != rc)
		goto error;

	printf("Sent request to 4chan.\n");
	char *all_json = receive_chunked_http(request_fd);
	ol_stack *matches = parse_catalog_json(all_json);

	/* This is where we'll queue up images to be downloaded. */
	ol_stack *images_to_download = malloc(sizeof(ol_stack));
	images_to_download->next = NULL;
	images_to_download->data = NULL;

	while (matches->next != NULL) {
		/* Pop our thread_match off the stack */
		thread_match *match = (thread_match*) spop(&matches);
		printf("Requesting %i...\n", match->thread_num);

		/* Template out a request to the 4chan API for it */
		/* (The 30 is because I don't want to find the length of the
		 * integer thread number) */
		const size_t thread_req_size = sizeof(THREAD_REQUEST) + 30;
		char templated_req[thread_req_size] = {0};
		snprintf(templated_req, thread_req_size, THREAD_REQUEST,
				match->board, match->thread_num);

		/* Send that shit over the wire */
		rc = send(request_fd, templated_req, strlen(templated_req), 0);

		char *thread_json = receive_chunked_http(request_fd);
		ol_stack *thread_matches = parse_thread_json(thread_json, match);
		while (thread_matches->next != NULL) {
			spush(&images_to_download, spop(&thread_matches));
		}
		free(thread_matches);
		free(thread_json);

		free(match);
	}
	free(matches);

	/* We don't need the API socket anymore. */
	close(request_fd);
	free(all_json);

	int thumb_request_fd = connect_to_host(FOURCHAN_THUMBNAIL_HOST);
	if (thumb_request_fd < 0)
		goto error;

	int image_request_fd = connect_to_host(FOURCHAN_IMAGE_HOST);
	if (image_request_fd < 0)
		goto error;

	/* Now actually download the images. */
	while (images_to_download->next != NULL) {
		post_match *p_match = (post_match *)spop(&images_to_download);
		printf("Downloading %s%.*s...\n", p_match->filename, 5, p_match->file_ext);

		/* Build and send the thumbnail request. */
		char thumb_request[256] = {0};
		snprintf(thumb_request, sizeof(thumb_request), THUMB_REQUEST,
				p_match->board, p_match->filename);
		send(thumb_request_fd, thumb_request, strlen(thumb_request), 0);

		/* Build and send the image request. */
		char image_request[256] = {0};
		snprintf(image_request, sizeof(image_request), IMAGE_REQUEST,
				p_match->board, p_match->filename, p_match->file_ext);
		send(image_request_fd, image_request, strlen(image_request), 0);

		char *raw_thumb_resp = receive_http(thumb_request_fd);
		char *raw_image_resp = receive_http(image_request_fd);

		const char *image = parse_image_from_http(raw_image_resp);
		const char *thumb_image = parse_image_from_http(raw_thumb_resp);

		/* TODO: Write responses to disk. */

		/* Don't need the post match anymore: */
		free(p_match);
	}
	free(images_to_download);
	close(thumb_request_fd);
	close(image_request_fd);

	return 0;

error:
	return -1;
}

