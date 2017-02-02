// vim: noet ts=4 sw=4
#pragma once
#include "common_defs.h"
#include "utils.h"

#define DEFAULT_NUM_THREADS 2

/* Used for sorting files inside of directories. */
struct file_and_time {
	char fname[MAX_IMAGE_FILENAME_SIZE];
	const time_t ctime;
};

int static_handler(const http_request *request, http_response *response);
int board_static_handler(const http_request *request, http_response *response);
int index_handler(const http_request *request, http_response *response);
int webm_handler(const http_request *request, http_response *response);
int by_alias_handler(const http_request *request, http_response *response);
int board_handler(const http_request *request, http_response *response);
int paged_board_handler(const http_request *request, http_response *response);
int favicon_handler(const http_request *request, http_response *response);
int robots_handler(const http_request *request, http_response *response);
int by_thread_handler(const http_request *request, http_response *response);
int url_search_handler(const http_request *request, http_response *response);
