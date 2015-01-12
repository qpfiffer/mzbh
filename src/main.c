// vim: noet ts=4 sw=4
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include "db.h"
#include "http.h"
#include "models.h"
#include "parse.h"
#include "logging.h"
#include "server.h"
#include "stack.h"

#define DEBUG 0
int main_sock_fd = 0;
pid_t bg_worker = 0;
const char *BOARDS[] = {"a", "b", "g", "gif", "e", "h", "sci", "v", "wsg"};

const char FOURCHAN_API_HOST[] = "a.4cdn.org";
const char FOURCHAN_THUMBNAIL_HOST[] = "t.4cdn.org";
const char FOURCHAN_IMAGE_HOST[] = "i.4cdn.org";

const char CATALOG_REQUEST[] =
	"GET /%s/catalog.json HTTP/1.1\r\n"
	"User-Agent: Mozilla/5.0 (Windows NT 5.1; rv:31.0) Gecko/20100101 Firefox/31.0\r\n"
	"Host: a.4cdn.org\r\n"
	"Accept: application/json\r\n\r\n";

const char THREAD_REQUEST[] =
	"GET /%s/thread/%i.json HTTP/1.1\r\n"
	"User-Agent: Mozilla/5.0 (Windows NT 5.1; rv:31.0) Gecko/20100101 Firefox/31.0\r\n"
	"Host: a.4cdn.org\r\n"
	"Accept: application/json\r\n\r\n";

const char IMAGE_REQUEST[] =
	"GET /%s/%s%.*s HTTP/1.1\r\n"
	"User-Agent: Mozilla/5.0 (Windows NT 5.1; rv:31.0) Gecko/20100101 Firefox/31.0\r\n"
	"Host: i.4cdn.org\r\n"
	"Accept: */*\r\n\r\n";

const char THUMB_REQUEST[] =
	"GET /%s/%ss.jpg HTTP/1.1\r\n"
	"User-Agent: Mozilla/5.0 (Windows NT 5.1; rv:31.0) Gecko/20100101 Firefox/31.0\r\n"
	"Host: t.4cdn.org\r\n"
	"Accept: */*\r\n\r\n";

static ol_stack *build_thread_index() {
	int request_fd = 0;
	ol_stack *images_to_download = NULL;
	request_fd = connect_to_host(FOURCHAN_API_HOST);
	if (request_fd < 0) {
		log_msg(LOG_ERR, "Could not connect to %s.", FOURCHAN_API_HOST);
		goto error;
	}
	log_msg(LOG_INFO, "Connected to %s.", FOURCHAN_API_HOST);

	/* This is where we'll queue up images to be downloaded. */
	images_to_download = malloc(sizeof(ol_stack));
	images_to_download->next = NULL;
	images_to_download->data = NULL;

	int i;
	for (i = 0; i < (sizeof(BOARDS)/sizeof(BOARDS[0])); i++) {
		const char *current_board = BOARDS[i];

		const size_t api_request_siz = strlen(CATALOG_REQUEST) + strnlen(current_board, MAX_BOARD_NAME_SIZE);
		char new_api_request[api_request_siz];
		memset(new_api_request, '\0', api_request_siz);

		snprintf(new_api_request, api_request_siz, CATALOG_REQUEST, current_board);
		int rc = send(request_fd, new_api_request, strlen(new_api_request), 0);
		if (strlen(new_api_request) != rc)
			goto error;

		log_msg(LOG_INFO, "Sent request to %s.", FOURCHAN_API_HOST);
		char *all_json = receive_chunked_http(request_fd);
		if (all_json == NULL) {
			log_msg(LOG_WARN, "Could not receive chunked HTTP from board for /%s/.", current_board);
			continue;
		}

		ol_stack *matches = parse_catalog_json(all_json, current_board);

		while (matches->next != NULL) {
			/* Pop our thread_match off the stack */
			thread_match *match = (thread_match*) spop(&matches);
			ensure_directory_for_board(match->board);

			log_msg(LOG_INFO, "/%s/ - Requesting thread %i...", current_board, match->thread_num);

			/* Template out a request to the 4chan API for it */
			/* (The 30 is because I don't want to find the length of the
			 * integer thread number) */
			const size_t thread_req_size = sizeof(THREAD_REQUEST) + strnlen(match->board, MAX_BOARD_NAME_SIZE) + 30;
			char templated_req[thread_req_size];
			memset(templated_req, '\0', thread_req_size);

			snprintf(templated_req, thread_req_size, THREAD_REQUEST,
					match->board, match->thread_num);

			/* Send that shit over the wire */
			rc = send(request_fd, templated_req, strlen(templated_req), 0);
			if (rc != strlen(templated_req)) {
				log_msg(LOG_ERR, "Could not send all of request.");
				continue;
			}

			char *thread_json = receive_chunked_http(request_fd);
			if (thread_json == NULL) {
				log_msg(LOG_WARN, "Could not receive chunked HTTP for thread. continuing.");
				/* Reopen and manipulate the socket. */
				close(request_fd);
				request_fd = connect_to_host(FOURCHAN_API_HOST);
				free(match);
				continue;
			}

			ol_stack *thread_matches = parse_thread_json(thread_json, match);
			while (thread_matches->next != NULL) {
				post_match *p_match = (post_match *)spop(&thread_matches);
				/* TODO: Don't add it to the images to download if we already
				 * have that image, with that size, from that board. */
				char fname[MAX_IMAGE_FILENAME_SIZE] = {0};
				int should_skip = get_non_colliding_image_file_path(fname, p_match);

				/* We already have that file. */
				if (should_skip) {
					free(p_match);
					continue;
				}

				/* Check if we have an existing alias for this file. */
				webm_alias *existing = get_aliased_image(fname);
				if (existing) {
					log_msg(LOG_INFO, "Found alias for '%s', skipping.", fname);
					free(p_match);
					free(existing);
					continue;
				}

				spush(&images_to_download, p_match);
			}
			free(thread_matches);
			free(thread_json);

			free(match);
		}
		free(matches);

		free(all_json);
	}
	/* We don't need the API socket anymore. */
	close(request_fd);

	return images_to_download;

error:
	if (images_to_download != NULL) {
		while (images_to_download->next != NULL) {
			post_match *_match = (post_match *)spop(&images_to_download);
			free(_match);
		}
		free(images_to_download);
	}
	close(request_fd);
	return NULL;
}

int download_images() {
	int thumb_request_fd = 0;
	int image_request_fd = 0;
	unsigned char *raw_thumb_resp = NULL;
	unsigned char *raw_image_resp = NULL;
	FILE *thumb_file = NULL;
	FILE *image_file = NULL;
	post_match *p_match = NULL;

	struct stat st = {0};
	if (stat(webm_location(), &st) == -1) {
		log_msg(LOG_WARN, "Creating webms directory %s.", webm_location());
		mkdir(webm_location(), 0755);
	}

	ol_stack *images_to_download = NULL;
	images_to_download = build_thread_index();
	if (images_to_download == NULL) {
		log_msg(LOG_WARN, "No images to download.");
		goto error;
	}

	/* Now actually download the images. */
	while (images_to_download->next != NULL) {
		thumb_request_fd = connect_to_host(FOURCHAN_THUMBNAIL_HOST);
		if (thumb_request_fd < 0) {
			log_msg(LOG_ERR, "Could not connect to thumbnail host.");
			goto error;
		}

		image_request_fd = connect_to_host(FOURCHAN_IMAGE_HOST);
		if (image_request_fd < 0) {
			log_msg(LOG_ERR, "Could not connect to image host.");
			goto error;
		}

		p_match = (post_match *)spop(&images_to_download);

		char image_filename[MAX_IMAGE_FILENAME_SIZE] = {0};
		int should_skip = get_non_colliding_image_file_path(image_filename, p_match);

		/* We already have that file. */
		if (should_skip) {
			free(p_match);
			continue;
		}

		char thumb_filename[MAX_IMAGE_FILENAME_SIZE] = {0};
		get_thumb_filename(thumb_filename, p_match);

		log_msg(LOG_INFO, "Downloading %s%.*s...", p_match->filename, 5, p_match->file_ext);

		/* Build and send the thumbnail request. */
		char thumb_request[256] = {0};
		snprintf(thumb_request, sizeof(thumb_request), THUMB_REQUEST,
				p_match->board, p_match->post_number);
		int rc = send(thumb_request_fd, thumb_request, strlen(thumb_request), 0);
		if (rc != strlen(thumb_request)) {
			log_msg(LOG_ERR, "Could not send all bytes to host while requesting thumbnail.");
			goto error;
		}

		/* Build and send the image request. */
		char image_request[256] = {0};
		snprintf(image_request, sizeof(image_request), IMAGE_REQUEST,
				p_match->board, p_match->post_number, (int)sizeof(p_match->file_ext), p_match->file_ext);
		rc = send(image_request_fd, image_request, strlen(image_request), 0);
		if (rc != strlen(image_request)) {
			log_msg(LOG_ERR, "Could not send all bytes to host while requesting image.");
			goto error;
		}

		size_t thumb_size = 0, image_size = 0;
		raw_thumb_resp = receive_http(thumb_request_fd, &thumb_size);
		raw_image_resp = receive_http(image_request_fd, &image_size);

		if (thumb_size <= 0 || image_size <= 0) {
			/* 4chan cut us off. This happens sometimes. Just sleep for a bit. */
			log_msg(LOG_WARN, "Hit API cutoff or whatever. Sleeping.");
			sleep(30);
			free(p_match);
			continue;
		}

		if (raw_thumb_resp == NULL) {
			log_msg(LOG_ERR, "No thumbnail received.");
			goto error;
		}

		if (raw_image_resp == NULL) {
			log_msg(LOG_ERR, "No image received.");
			goto error;
		}

		/* Write thumbnail to disk. */
		thumb_file = fopen(thumb_filename, "wb");
		if (!thumb_file || ferror(thumb_file)) {
			log_msg(LOG_ERR, "Could not open thumbnail file: %s", thumb_filename);
			perror(NULL);
			goto error;
		}
		const size_t rt_written = fwrite(raw_thumb_resp, 1, thumb_size, thumb_file);
		log_msg(LOG_INFO, "Wrote %i bytes of thumbnail to disk.", rt_written);
		if (rt_written <= 0 || ferror(thumb_file)) {
			log_msg(LOG_WARN, "Could not write thumbnail to disk: %s", thumb_filename);
			perror(NULL);
			goto error;
		}
		fclose(thumb_file);
		thumb_file = 0;

		image_file = fopen(image_filename, "wb");
		if (!image_file || ferror(image_file)) {
			log_msg(LOG_ERR, "Could not open image file: %s", image_filename);
			perror(NULL);
			goto error;
		}
		const size_t iwritten = fwrite(raw_image_resp, 1, image_size, image_file);
		if (iwritten <= 0 || ferror(image_file)) {
			log_msg(LOG_WARN, "Could not write image to disk: %s", image_filename);
			perror(NULL);
			goto error;
		}
		log_msg(LOG_INFO, "Wrote %i bytes of image to disk.", iwritten);
		fclose(image_file);
		image_file = 0;

		char fname_plus_extension[MAX_IMAGE_FILENAME_SIZE] = {0};
		get_non_colliding_image_filename(fname_plus_extension, p_match);
		int added = add_image_to_db(image_filename, fname_plus_extension, p_match->board);
		if (!added) {
			log_msg(LOG_WARN, "Could not add image to database. Continuing...");
		}

		/* Don't need the post match anymore: */
		free(p_match);
		p_match = NULL;
		free(raw_image_resp);
		raw_image_resp = NULL;
		free(raw_thumb_resp);
		raw_thumb_resp = NULL;

		close(thumb_request_fd);
		close(image_request_fd);
	}
	free(images_to_download);
	log_msg(LOG_INFO, "Downloaded all images.");

	return 0;

error:
	if (thumb_request_fd)
		close(thumb_request_fd);

	if (image_request_fd)
		close(image_request_fd);

	if (p_match)
		free(p_match);

	if (images_to_download != NULL) {
		while (images_to_download->next != NULL) {
			post_match *_match = (post_match *)spop(&images_to_download);
			free(_match);
		}
		free(images_to_download);
	}

	if (raw_thumb_resp)
		free(raw_thumb_resp);
	if (raw_image_resp)
		free(raw_image_resp);
	if (thumb_file != NULL)
		fclose(thumb_file);
	if (image_file != NULL)
		fclose(image_file);
	return -1;
}

void term(int signum) {
	kill(bg_worker, signum);
	close(main_sock_fd);
	exit(1);
}

void background_work(int debug) {
start:
	if (download_images() != 0) {
		log_msg(LOG_WARN, "Something went wrong while downloading images.");
	}
	sleep(600);

	/* We fork and use a goto here to make the kernel clean up
	 * my memory mess. */
	bg_worker = fork();
	if (bg_worker == 0) {
		log_msg(LOG_INFO, "BGWorker started.");
		goto start;
	} else if (bg_worker == -1) {
		log_msg(LOG_ERR, "Fork failed. Wtf?");
	}
	log_msg(LOG_INFO, "BGWorker exiting.");
	exit(0);
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

	int num_threads = DEFAULT_NUM_THREADS;
	int start_bg_worker_t = 1;
	int i;
	for (i = 1; i < argc; i++) {
		const char *cur_arg = argv[i];
		if (strncmp(cur_arg, "-t", strlen("-t")) == 0) {
			if ((i + 1) < argc) {
				num_threads = strtol(argv[++i], NULL, 10);
				if (num_threads <= 0) {
					log_msg(LOG_ERR, "Thread count must be at least 1.");
					return -1;
				}
			} else {
				log_msg(LOG_ERR, "Not enough arguments to -t.");
				return -1;
			}
		}

		if (strncmp(cur_arg, "--serve", strlen("--serve")) == 0)
			start_bg_worker_t = 0;
	}

	if (start_bg_worker_t && start_bg_worker(DEBUG) != 0)
		return -1;

	int rc = 0;
	if ((rc = http_serve(main_sock_fd, num_threads)) != 0) {
		term(SIGTERM);
		log_msg(LOG_ERR, "Could not start HTTP service.");
		return rc;
	}
	return 0;
}
