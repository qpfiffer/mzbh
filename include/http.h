// vim: noet ts=4 sw=4
#pragma once

#define SOCK_RECV_MAX 4096
#define SELECT_TIMEOUT 5

int connect_to_host(const char *host);
int connect_to_host_with_port(const char *host, const char *port);

char *receive_http(const int request_fd, size_t *out);
char *receive_http_with_timeout(const int request_fd, const int timeout, size_t *out);
char *receive_chunked_http(const int request_fd);
