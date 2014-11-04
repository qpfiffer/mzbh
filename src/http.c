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
#include "logging.h"

const int SELECT_TIMEOUT = 5;
const char *BOARDS[] = {"a", "b", "gif", "e", "h", "v", "wsg"};
const char WEBMS_DIR[] = "./webms";

const char FOURCHAN_API_HOST[] = "a.4cdn.org";
const char FOURCHAN_THUMBNAIL_HOST[] = "t.4cdn.org";
const char FOURCHAN_IMAGE_HOST[] = "i.4cdn.org";

const char CATALOG_REQUEST[] =
	"GET /%s/catalog.json HTTP/1.1\r\n"
	"User-Agent: Mozilla/5.0 (Windows NT 5.1; rv:31.0) Gecko/20100101 Firefox/31.0\r\n"
	"Host: a.4cdn.org\r\n"
	"Accept: application/json\r\n\r\n";

const char THREAD_REQUEST[] =
	"GET /%s/thread/%i.json HTTP/1.1\r\n"
	"User-Agent: Mozilla/5.0 (Windows NT 5.1; rv:31.0) Gecko/20100101 Firefox/31.0\r\n"
	"Host: a.4cdn.org\r\n"
	"Accept: application/json\r\n\r\n";

const char IMAGE_REQUEST[] =
	"GET /%s/%s%.*s HTTP/1.1\r\n"
	"User-Agent: Mozilla/5.0 (Windows NT 5.1; rv:31.0) Gecko/20100101 Firefox/31.0\r\n"
	"Host: i.4cdn.org\r\n"
	"Accept: */*\r\n\r\n";

const char THUMB_REQUEST[] =
	"GET /%s/%ss.jpg HTTP/1.1\r\n"
	"User-Agent: Mozilla/5.0 (Windows NT 5.1; rv:31.0) Gecko/20100101 Firefox/31.0\r\n"
	"Host: t.4cdn.org\r\n"
	"Accept: */*\r\n\r\n";

static char *receive_chunked_http(const int request_fd) {
	char *raw_buf = malloc(0);
	size_t buf_size = 0;
	int times_read = 0;

	fd_set chan_fds;
	FD_ZERO(&chan_fds);
	FD_SET(request_fd, &chan_fds);
	const int maxfd = request_fd;

	while (1) {
		times_read++;

		/* Wait for data to be read. */
		struct timeval tv = {
			.tv_sec = SELECT_TIMEOUT,
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

		recv(request_fd, raw_buf + old_offset, count, 0);
	}
	/* printf("Full message is %s\n.", raw_buf); */
	/* Check for a 200: */
	if (strstr(raw_buf, "200") == NULL)
		return NULL;

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
	/* printf("The total json size is %zu.\n", json_total); */
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
	if (request_fd < 0) {
		log_msg(LOG_ERR, "Could not create socket.");
		goto error;
	}

	int opt = 1;
	setsockopt(request_fd, SOL_SOCKET, SO_REUSEADDR, (void*) &opt, sizeof(opt));

	int rc = connect(request_fd, res->ai_addr, res->ai_addrlen);
	if (rc == -1) {
		log_msg(LOG_ERR, "Could not connect to host %s.", host);
		goto error;
	}

	freeaddrinfo(res);
	return request_fd;

error:
	close(request_fd);
	return -1;
}

static char *receive_http(const int request_fd, size_t *out) {
	char *raw_buf = malloc(0);
	size_t buf_size = 0;
	int times_read = 0;

	fd_set chan_fds;
	FD_ZERO(&chan_fds);
	FD_SET(request_fd, &chan_fds);
	const int maxfd = request_fd;

	while (1) {
		times_read++;

		/* Wait for data to be read. */
		struct timeval tv = {
			.tv_sec = SELECT_TIMEOUT,
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

		recv(request_fd, raw_buf + old_offset, count, 0);
	}
	/* printf("Full message is %s\n.", raw_buf); */
	/* Check for a 200: */
	if (strstr(raw_buf, "200") == NULL)
		return NULL;

	/* 4Chan throws us data as chunk-encoded HTTP. Rad. */
	char *header_end = strstr(raw_buf, "\r\n\r\n");
	char *cursor_pos = header_end  + (sizeof(char) * 4);

	size_t result_size = 0;
	char *offset_for_clength = strstr(raw_buf, "Content-Length: ");
	if (offset_for_clength != NULL) {
		char siz_buf[128] = {0};
		int i = 0;

		const char *to_read = offset_for_clength + strlen("Content-Length: ");
		while (to_read[i] != ';' && i < sizeof(siz_buf)) {
			siz_buf[i] = to_read[i];
			i++;
		}
		result_size = strtol(siz_buf, NULL, 10);
	}
	log_msg(LOG_INFO, "Received %lu bytes.", result_size);

	char *to_return = malloc(result_size);
	strncpy(to_return, cursor_pos, result_size);
	*out = result_size;
	free(raw_buf);

	return cursor_pos;
}

static void ensure_directory_for_board(const char *board) {
	/* Long enough for WEBMS_DIR, a /, the board and a NULL terminator */
	const size_t buf_siz = strlen(WEBMS_DIR) + sizeof(char) * 2 + strnlen(board, BOARD_STR_LEN);
	char to_create[buf_siz];
	memset(to_create, '\0', buf_siz);

	/* ./webms/b */
	snprintf(to_create, buf_siz, "%s/%s", WEBMS_DIR, board);

	struct stat st = {0};
	if (stat(to_create, &st) == -1) {
		log_msg(LOG_WARN, "Creating directory %s.", to_create);
		mkdir(to_create, 0755);
	}
}

static ol_stack *build_thread_index() {
	int request_fd = connect_to_host(FOURCHAN_API_HOST);
	if (request_fd < 0)
		goto error;
	log_msg(LOG_INFO, "Connected to %s.", FOURCHAN_API_HOST);

	/* This is where we'll queue up images to be downloaded. */
	ol_stack *images_to_download = malloc(sizeof(ol_stack));
	images_to_download->next = NULL;
	images_to_download->data = NULL;

	int i;
	for (i = 0; i < (sizeof(BOARDS)/sizeof(BOARDS[0])); i++) {
		const char *current_board = BOARDS[i];

		const size_t api_request_siz = strlen(CATALOG_REQUEST) + strnlen(current_board, BOARD_STR_LEN);
		char new_api_request[api_request_siz];
		memset(new_api_request, '\0', api_request_siz);

		snprintf(new_api_request, api_request_siz, CATALOG_REQUEST, current_board);
		int rc = send(request_fd, new_api_request, strlen(new_api_request), 0);
		if (strlen(new_api_request) != rc)
			goto error;

		log_msg(LOG_INFO, "Sent request to %s.", FOURCHAN_API_HOST);
		char *all_json = receive_chunked_http(request_fd);
		if (all_json == NULL)
			goto error;

		ol_stack *matches = parse_catalog_json(all_json, current_board);

		while (matches->next != NULL) {
			/* Pop our thread_match off the stack */
			thread_match *match = (thread_match*) spop(&matches);
			ensure_directory_for_board(match->board);

			log_msg(LOG_INFO, "Requesting thread %i...", match->thread_num);

			/* Template out a request to the 4chan API for it */
			/* (The 30 is because I don't want to find the length of the
			 * integer thread number) */
			const size_t thread_req_size = sizeof(THREAD_REQUEST) + strnlen(match->board, BOARD_STR_LEN) + 30;
			char templated_req[thread_req_size];
			memset(templated_req, '\0', thread_req_size);

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

		free(all_json);
	}
	/* We don't need the API socket anymore. */
	close(request_fd);

	return images_to_download;

error:
	close(request_fd);
	return NULL;
}

int download_images() {
	int thumb_request_fd = 0;
	int image_request_fd = 0;

	struct stat st = {0};
	if (stat(WEBMS_DIR, &st) == -1) {
		log_msg(LOG_WARN, "Creating webms directory %s.", WEBMS_DIR);
		mkdir(WEBMS_DIR, 0755);
	}

	ol_stack *images_to_download = build_thread_index();
	if (!images_to_download) {
		log_msg(LOG_WARN, "No images to download.");
		goto error;
	}

	thumb_request_fd = connect_to_host(FOURCHAN_THUMBNAIL_HOST);
	if (thumb_request_fd < 0) {
		log_msg(LOG_ERR, "Could not connect to thumbnail host.");
		goto error;
	}

	image_request_fd = connect_to_host(FOURCHAN_IMAGE_HOST);
	if (image_request_fd < 0) {
		log_msg(LOG_ERR, "Could not connect to image host.");
		goto error;
	}

	/* Now actually download the images. */
	while (images_to_download->next != NULL) {
		post_match *p_match = (post_match *)spop(&images_to_download);

		char image_filename[128] = {0};
		snprintf(image_filename, 128, "%s/%s/%s%.*s",
				WEBMS_DIR, p_match->board, p_match->filename,
				(int)sizeof(p_match->file_ext), p_match->file_ext);

		/* We already have this file, don't need to download it again. */
		struct stat ifname = {0};
		if (stat(image_filename, &ifname) != -1 && ifname.st_size > 0) {
			log_msg(LOG_INFO, "Skipping %s.", image_filename);
			free(p_match);
			continue;
		}

		char thumb_filename[128] = {0};
		snprintf(thumb_filename, 128, "%s/%s/thumb_%s.jpg",
				WEBMS_DIR, p_match->board, p_match->filename);

		log_msg(LOG_INFO, "Downloading %s%.*s...", p_match->filename, 5, p_match->file_ext);

		/* Build and send the thumbnail request. */
		char thumb_request[256] = {0};
		snprintf(thumb_request, sizeof(thumb_request), THUMB_REQUEST,
				p_match->board, p_match->tim);
		int rc = send(thumb_request_fd, thumb_request, strlen(thumb_request), 0);
		if (rc != strlen(thumb_request)) {
			log_msg(LOG_ERR, "Could not send all bytes to host while requesting thumbnail.");
			goto error;
		}

		/* Build and send the image request. */
		char image_request[256] = {0};
		snprintf(image_request, sizeof(image_request), IMAGE_REQUEST,
				p_match->board, p_match->tim, (int)sizeof(p_match->file_ext), p_match->file_ext);
		rc = send(image_request_fd, image_request, strlen(image_request), 0);
		if (rc != strlen(image_request)) {
			log_msg(LOG_ERR, "Could not send all bytes to host while requesting image.");
			goto error;
		}

		size_t thumb_size, image_size;
		char *raw_thumb_resp = receive_http(thumb_request_fd, &thumb_size);
		char *raw_image_resp = receive_http(image_request_fd, &image_size);

		if (thumb_size <= 0 || image_size <= 0) {
			/* 4chan cut us off. This happens sometimes. Just sleep for a bit. */
			log_msg(LOG_WARN, "Hit API cutoff or whatever. Sleeping.");
			sleep(300);
			/* Now we try again: */
			raw_thumb_resp = receive_http(thumb_request_fd, &thumb_size);
			raw_image_resp = receive_http(image_request_fd, &image_size);
		}

		if (raw_thumb_resp == NULL) {
			log_msg(LOG_ERR, "No thumbnail received.");
			goto error;
		}

		if (raw_image_resp == NULL) {
			log_msg(LOG_ERR, "No image received.");
			goto error;
		}

		/* Write thumbnail to disk. */
		FILE *thumb_file;
		thumb_file = fopen(thumb_filename, "wb");
		const size_t rt_written = fwrite(raw_thumb_resp, 1, thumb_size, thumb_file);
		log_msg(LOG_INFO, "Wrote %i bytes of thumbnail to disk.", rt_written);
		fclose(thumb_file);

		FILE *image_file;
		image_file = fopen(image_filename, "wb");
		const size_t iwritten = fwrite(raw_image_resp, 1, image_size, image_file);
		log_msg(LOG_INFO, "Wrote %i bytes of image to disk.", iwritten);
		fclose(image_file);

		/* Don't need the post match anymore: */
		free(p_match);
	}
	free(images_to_download);
	close(thumb_request_fd);
	close(image_request_fd);

	return 0;

error:
	if (thumb_request_fd)
		close(thumb_request_fd);

	if (image_request_fd)
		close(image_request_fd);
	return -1;
}

