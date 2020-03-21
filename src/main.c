// vim: noet ts=4 sw=4
#ifdef __clang__
	#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#endif
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <curl/curl.h>

#include <38-moths/38-moths.h>

#include "db.h"
#include "http.h"
#include "models.h"
#include "parse.h"
#include "server.h"
#include "stack.h"
#include "utils.h"

int main_sock_fd = 0;

void term(int signum) {
	UNUSED(signum);
	close(main_sock_fd);
	exit(1);
}

static const m38_route all_routes[] = {
	{"GET", "robots_txt", "^/robots.txt$", 0, &robots_handler, &m38_mmap_cleanup},
	{"GET", "favicon_ico", "^/favicon.ico$", 0, &favicon_handler, &m38_mmap_cleanup},
	{"GET", "generic_static", "^/static/[a-zA-Z0-9/_-]*\\.[a-zA-Z]*$", 0, &static_handler, &m38_mmap_cleanup},
	{"GET", "user_uploaded_thumbs", "^/static/user_thumbs/[a-zA-Z0-9/_-]*\\.[a-zA-Z]*$", 0, &user_thumbs_static_handler, &m38_mmap_cleanup},
	{"POST", "search_by_url", "^/search/url.json$", 0, &url_search_handler, &m38_heap_cleanup},
	{"GET", "board_handler_no_num", "^/chug/([a-zA-Z]*)$", 1, &board_handler, &m38_heap_cleanup},
	{"GET", "paged_board_handler", "^/chug/([a-zA-Z]*)/([0-9]*)$", 2, &paged_board_handler, &m38_heap_cleanup},
	{"GET", "webm_handler", "^/slurp/([a-zA-Z]*)/((.*)(.webm|.jpg))$", 2, &webm_handler, &m38_heap_cleanup},
	{"GET", "board_static_handler", "^/chug/([a-zA-Z]*)/((.*)(.webm|.jpg))$", 2, &board_static_handler, &m38_mmap_cleanup},
	{"GET", "by_alias_handler", "^/by/alias/([0-9]*)$", 1, &by_alias_handler, &m38_heap_cleanup},
	{"GET", "by_thread_handler", "^/by/thread/([A-Z]*[a-z]*[0-9]*)$", 1, &by_thread_handler, &m38_heap_cleanup},
	{"GET", "api_index_stats", "^/api/index_stats$", 1, &api_index_stats, &m38_heap_cleanup},
	{"GET", "root_handler", "^/$", 0, &index_handler, &m38_heap_cleanup},
};

static m38_app waifu_app = {
	.main_sock_fd = &main_sock_fd,
	.port = 8666,
	.num_threads = DEFAULT_NUM_THREADS,
	.routes = all_routes,
	.num_routes = sizeof(all_routes)/sizeof(all_routes[0]),
};

int main(int argc, char *argv[]) {
	signal(SIGTERM, term);
	signal(SIGINT, term);
	signal(SIGKILL, term);
	signal(SIGCHLD, SIG_IGN);

	curl_global_init(CURL_GLOBAL_ALL);

	int num_threads = DEFAULT_NUM_THREADS;
	int i;
	for (i = 1; i < argc; i++) {
		const char *cur_arg = argv[i];
		if (strncmp(cur_arg, "-t", strlen("-t")) == 0) {
			if ((i + 1) < argc) {
				num_threads = strtol(argv[++i], NULL, 10);
				if (num_threads <= 0) {
					m38_log_msg(LOG_ERR, "Thread count must be at least 1.");
					return -1;
				}
			} else {
				m38_log_msg(LOG_ERR, "Not enough arguments to -t.");
				return -1;
			}
		}
	}

	int rc = 0;
	waifu_app.num_threads = num_threads;
	if ((rc = m38_http_serve(&waifu_app)) != 0) {
		term(SIGTERM);
		m38_log_msg(LOG_ERR, "Could not start HTTP service.");
		return rc;
	}
	return 0;
}
