// vim: noet ts=4 sw=4
#pragma once
#include "utils.h"

#define DEFAULT_NUM_THREADS 2

int http_serve(int main_sock_fd, const int num_threads);
