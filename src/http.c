// vim: noet ts=4 sw=4
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "http.h"
#include "parse.h"

const char FOURCHAN_API_HOST[] = "a.4cdn.org";

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

char *receive_chunked_http(const int request_fd) {
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

int download_images() {
	struct addrinfo hints = {0};
	struct addrinfo *res = NULL;
	int request_fd;

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	getaddrinfo(FOURCHAN_API_HOST, "80", &hints, &res);

	request_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (request_fd < 0)
		goto error;

	int opt = 1;
	setsockopt(request_fd, SOL_SOCKET, SO_REUSEADDR, (void*) &opt, sizeof(opt));

	int rc = connect(request_fd, res->ai_addr, res->ai_addrlen);
	if (rc == -1)
		goto error;

	printf("Connected to 4chan.\n");
	rc = send(request_fd, B_API_REQUEST, strlen(B_API_REQUEST), 0);
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

	/* Now actually download the images. */
	while (images_to_download->next != NULL) {
		thread_match *t_match = (thread_match *)spop(&images_to_download);
		free(t_match);
	}
	free(images_to_download);

	/* So at this point we just read shit into an ever expanding buffer. */
	printf("BGWorker exiting.\n");

	free(all_json);
	close(request_fd);
	freeaddrinfo(res);
	return 0;

error:
	printf("ERROR.\n");
	close(request_fd);
	return -1;
}

char *receive_http(const int request_fd) {
	return NULL;
}
