// vim: noet ts=4 sw=4
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "http.h"
#include "server.h"

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

int main(int argc, char *argv[]) {
	signal(SIGTERM, term);
	signal(SIGINT, term);
	signal(SIGKILL, term);
	signal(SIGCHLD, SIG_IGN);
	if (start_bg_worker(DEBUG) != 0)
		return -1;
	int rc = 0;
	if ((rc = http_serve(main_sock_fd)) != 0) {
		term(SIGTERM);
		printf("COULD NOT SERVE HTTP\n");
		return rc;
	}
	return 0;
}
