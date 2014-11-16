// vim: noet ts=4 sw=4
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "http.h"
#include "server.h"
#include "logging.h"

#define DEBUG 0
int main_sock_fd = 0;
pid_t bg_worker = 0;

void term(int signum) {
	kill(bg_worker, signum);
	close(main_sock_fd);
	exit(1);
}

void background_work(int debug) {
	log_msg(LOG_INFO, "BGWorker started.");
	while (1) {
		if (download_images() != 0) {
			log_msg(LOG_WARN, "Something went wrong while downloading images.");
		}
		sleep(600);
	}
	log_msg(LOG_INFO, "BGWorker exiting.");
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
		log_msg(LOG_ERR, "Could not start HTTP service.");
		return rc;
	}
	return 0;
}
