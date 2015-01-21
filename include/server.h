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

int http_serve(int main_sock_fd, const int num_threads);
