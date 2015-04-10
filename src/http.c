// vim: noet ts=4 sw=4
#ifdef __clang__
	#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#endif
#include <arpa/inet.h>
#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "utils.h"
#include "http.h"
#include "parse.h"
#include "logging.h"
#include "models.h"

char *receive_chunked_http(const int request_fd) {
	char *raw_buf = NULL;
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
		if (raw_buf != NULL) {
			char *new_buf = realloc(raw_buf, buf_size);
			if (new_buf != NULL) {
				raw_buf = new_buf;
			} else {
				goto error;
			}
		} else {
			raw_buf = calloc(1, buf_size);
		}

		int recvd = recv(request_fd, raw_buf + old_offset, count, 0);
		if (recvd != count) {
			log_msg(LOG_WARN, "Could not receive entire message.");
		}
	}
	/* printf("Full message is %s\n.", raw_buf); */
	/* Check for a 200: */
	if (raw_buf == NULL || strstr(raw_buf, "200") == NULL) {
		log_msg(LOG_WARN, "Chunked: Could not find 200 return code in response.");
		goto error;
	}

	/* 4Chan throws us data as chunk-encoded HTTP. Rad. */
	char *header_end = strstr(raw_buf, "\r\n\r\n");
	if (header_end == NULL) {
		log_msg(LOG_ERR, "Could not find end of header in initial chunk.");
		goto error;
	}
	char *cursor_pos = header_end  + (sizeof(char) * 4);

	size_t json_total = 0;
	char *json_buf = NULL;

	while (1) {
		/* This is where the data begins. */
		char *chunk_size_start = cursor_pos;
		char *chunk_size_end = strstr(chunk_size_start, "\r\n");
		if (chunk_size_end == NULL) {
			log_msg(LOG_ERR, "Could not find '\r\n' in chunk.");
			goto error;
		}
		const int chunk_size_end_oft = chunk_size_end - chunk_size_start;

		/* We cheat a little and set the first \r to a \0 so strtol will
		 * do the right thing. */
		chunk_size_start[chunk_size_end_oft] = '\0';
		const long chunk_size = strtol(chunk_size_start, NULL, 16);

		if ((chunk_size == LONG_MIN || chunk_size == LONG_MAX) && errno == ERANGE) {
			log_msg(LOG_ERR, "Could not parse out chunk size.");
			goto error;
		}

		/* printf("Chunk size is %i. Thing is: %s.\n", chunk_size, chunk_size_start); */
		/* The chunk string, the \r\n after it, the chunk itself and then another \r\n: */
		cursor_pos += chunk_size + chunk_size_end_oft + 4;

		/* Copy the json into a pure buffer: */
		unsigned int old_offset = json_total;
		json_total += chunk_size;
		if (json_total >= old_offset) {
			if (json_buf != NULL) {
				char *new_buf = realloc(json_buf, json_total);
				if (new_buf != NULL) {
					json_buf = new_buf;
				} else {
					goto error;
				}
			} else {
				json_buf = calloc(1, json_total);
			}
		}
		/* Copy it from after the <chunk_size>\r\n to the end of the chunk. */
		memcpy(json_buf + old_offset, chunk_size_end + 2, chunk_size);
		/* Stop reading if we am play gods: */
		if ((unsigned int)(cursor_pos - raw_buf) > buf_size || chunk_size <= 0)
			break;
	}
	/* printf("The total json size is %zu.\n", json_total); */
	/* printf("JSON:\n%s", json_buf); */
	free(raw_buf);

	char *new_buf = realloc(json_buf, json_total + 1);
	if (new_buf == NULL) {
		free(json_buf);
		return NULL;
	}

	new_buf[json_total] = '\0';
	return new_buf;

error:
	if (raw_buf != NULL)
		free(raw_buf);
	return NULL;
}

