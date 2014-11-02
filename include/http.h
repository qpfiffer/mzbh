// vim: noet ts=4 sw=4
#pragma once

#define SOCK_RECV_MAX 4096

char *receive_chunked_http(const int request_fd);
char *receive_http(const int request_fd);
int new_API_request();
