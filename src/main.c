/* vim: noet ts=4 sw=4 */
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#define DEBUG 0

int main_sock_fd = 0;

int start_bg_worker(int debug) {
	return 0;
}

int http_serve() {
	main_sock_fd = socket(PF_INET, SOCK_STREAM, 0);
	if (main_sock_fd < 0)
		return -1;

	struct sockaddr_in hints = {0};
	hints.sin_family		 = AF_INET;
	hints.sin_port			 = htons(8080);
	hints.sin_addr.s_addr	 = htonl(INADDR_LOOPBACK);

	int rc = bind(main_sock_fd, (struct sockaddr *)&hints, sizeof(hints));
	if (rc < 0)
		return -1;

	rc = listen(main_sock_fd, 0);
	if (rc < 0)
		return -1;

	close(main_sock_fd);
	return 0;
}

int main(int argc, char *argv[]) {
	if (!start_bg_worker(DEBUG))
		return -1;
	if (!http_serve())
		return -1;
	return 0;
}
