// vim: noet ts=4 sw=4
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>

#include "http.h"

#define DEBUG 0
int main_sock_fd = 0;

void background_work(int debug) {
	printf("BGWorker chuggin'\n");
	if (download_images() != 0)
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
