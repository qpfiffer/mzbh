// vim: noet ts=4 sw=4
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "http.h"

#define DEBUG 0
int main_sock_fd = 0;
pid_t bg_worker = 0;

void term(int signum) {
	kill(bg_worker, signum);
	close(main_sock_fd);
	exit(1);
}

void background_work(int debug) {
	printf("BGWorker chuggin'\n");
	while (1) {
		if (download_images() != 0) {
			printf("Something went wrong when attempting to download images.\n");
			return;
		}
		sleep(600);
	}
	printf("BGWorker exiting.\n");
}

int start_bg_worker(int debug) {
	bg_worker = fork();
	if (bg_worker == 0) {
		background_work(debug);
	} else if (bg_worker < 0) {
		return -1;
	}
	return 0;
}

int http_serve() {
	int rc = -1;
	main_sock_fd = socket(PF_INET, SOCK_STREAM, 0);
	if (main_sock_fd <= 0)
		goto error;

	int opt = 1;
	setsockopt(main_sock_fd, SOL_SOCKET, SO_REUSEADDR, (void*) &opt, sizeof(opt));

	struct sockaddr_in hints = {0};
	hints.sin_family		 = AF_INET;
	hints.sin_port			 = htons(8080);
	hints.sin_addr.s_addr	 = htonl(INADDR_LOOPBACK);

	rc = bind(main_sock_fd, (struct sockaddr *)&hints, sizeof(hints));
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
	perror("Socket error");
	close(main_sock_fd);
	return rc;
}

int main(int argc, char *argv[]) {
	signal(SIGTERM, term);
	signal(SIGINT, term);
	signal(SIGKILL, term);
	if (start_bg_worker(DEBUG) != 0)
		return -1;
	int rc = 0;
	if ((rc = http_serve()) != 0) {
		term(SIGTERM);
		printf("COULD NOT SERVE HTTP\n");
		return rc;
	}
	return 0;
}
