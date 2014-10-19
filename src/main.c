// vim: noet ts=4 sw=4
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEBUG 0
#define SOCK_RECV_MAX 4096

const char FOURCHAN_API_HOST[] = "a.4cdn.org";
int main_sock_fd = 0;

const char API_REQUEST[] =
	"GET /b/catalog.json HTTP/1.1\r\n"
	"User-Agent: Mozilla/5.0 (Windows NT 5.1; rv:31.0) Gecko/20100101 Firefox/31.0\r\n"
	"Host: a.4cdn.org\r\n"
	"Accept text/html\r\n\r\n";

char *get_catalog(const int request_fd) {
	size_t msg_siz = SOCK_RECV_MAX;
	char *msg = malloc(SOCK_RECV_MAX);
	int num_bytes_read = 0;
	num_bytes_read = recv(request_fd, msg, SOCK_RECV_MAX, 0);

	/* 4Chan throws us data as chunk-encoded HTTP. Rad. */
	char *header_end = strstr(msg, "\r\n\r\n");
	/* This is where the data begins. */
	char *chunk_size_start = header_end + (sizeof(char) * 4);
	char *chunk_size_end = strstr(chunk_size_start, "\r\n");

	/* We cheat a little and set the first \r to a \0 so strtol will
	 * do the right thing. */
	chunk_size_start[chunk_size_end - chunk_size_start] = '\0';
	chunk_size_end = NULL;
	size_t chunk_size = strtol(chunk_size_start, NULL, 16);

	return msg;
}

int new_API_request() {
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
	rc = send(request_fd, API_REQUEST, strlen(API_REQUEST), 0);
	if (strlen(API_REQUEST) != rc)
		goto error;

	printf("Sent request to 4chan.\n");
	char *all_json = get_catalog(request_fd);

	free(all_json);
	close(request_fd);
	return 0;

error:
	printf("ERROR.\n");
	close(request_fd);
	return -1;
}

void background_work(int debug) {
	printf("BGWorker chuggin'\n");
	if (new_API_request() != 0)
		return;
}

int start_bg_worker(int debug) {
	pid_t PID = fork();
	if (PID == 0) {
		background_work(debug);
	} else if (PID < 0) {
		return -1;
	}
	return 0;
}

int http_serve() {
	main_sock_fd = socket(PF_INET, SOCK_STREAM, 0);
	if (main_sock_fd < 0)
		goto error;

	int opt = 1;
	setsockopt(main_sock_fd, SOL_SOCKET, SO_REUSEADDR, (void*) &opt, sizeof(opt));

	struct sockaddr_in hints = {0};
	hints.sin_family		 = AF_INET;
	hints.sin_port			 = htons(8080);
	hints.sin_addr.s_addr	 = htonl(INADDR_LOOPBACK);

	int rc = bind(main_sock_fd, (struct sockaddr *)&hints, sizeof(hints));
	if (rc < 0)
		goto error;

	rc = listen(main_sock_fd, 0);
	if (rc < 0)
		goto error;

	struct sockaddr_storage their_addr = {0};
	socklen_t sin_size = sizeof(their_addr);
	int new_fd = accept(main_sock_fd, (struct sockaddr *)&their_addr, &sin_size);
	if (new_fd == -1)

	close(main_sock_fd);
	return 0;

error:
	close(main_sock_fd);
	return -1;
}

int main(int argc, char *argv[]) {
	if (start_bg_worker(DEBUG) != 0)
		return -1;
	if (http_serve() != 0)
		return -1;
	return 0;
}
