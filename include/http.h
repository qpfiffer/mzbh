// vim: noet ts=4 sw=4
#pragma once

#define SOCK_RECV_MAX 4096
#define SELECT_TIMEOUT 5
#include "common_defs.h"

char *receive_chunked_http(const int request_fd);
size_t download_sent_webm_url(const char *url, const char filename[static MAX_IMAGE_FILENAME_SIZE], char outpath[static MAX_IMAGE_FILENAME_SIZE]);
