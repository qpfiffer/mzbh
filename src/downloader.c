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
#include <oleg-http/http.h>

#include "db.h"
#include "http.h"
#include "models.h"
#include "parse.h"
#include "stack.h"
#include "utils.h"

const char *BOARDS[] = {"a", "b", "fit", "g", "gif", "e", "h", "o", "n", "r", "s", "sci", "soc", "v"};
//const char *BOARDS[] = {"a", "b"};

const char FOURCHAN_API_HOST[] = "a.4cdn.org";
const char FOURCHAN_THUMBNAIL_HOST[] = "t.4cdn.org";
const char FOURCHAN_IMAGE_HOST[] = "i.4cdn.org";

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

struct MemoryStruct {
	char *memory;
	size_t size;
};

static size_t write_memory_callback(void *contents,size_t size,
									size_t nmemb, void *userp) {
	size_t realsize = size * nmemb;
	struct MemoryStruct *mem = (struct MemoryStruct *)userp;

	mem->memory = realloc(mem->memory, mem->size + realsize + 1);
	if(mem->memory == NULL) {
		m38_log_msg(LOG_ERR, "Not enough memory (realloc returned NULL)");
		return 0;
	}

	memcpy(&(mem->memory[mem->size]), contents, realsize);
	mem->size += realsize;
	mem->memory[mem->size] = 0;

	return realsize;
}

char *get_json(const char *url) {
	CURL *curl_handle;
	CURLcode res;

	struct MemoryStruct chunk;

	chunk.memory = malloc(1);
	chunk.size = 0;

	curl_handle = curl_easy_init();
	curl_easy_setopt(curl_handle, CURLOPT_URL, url);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_memory_callback);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
	curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
	res = curl_easy_perform(curl_handle);

	if (res != CURLE_OK) {
		const char *err = curl_easy_strerror(res);
		m38_log_msg(LOG_WARN, "Could not receive chunked HTTP from board: %s", err);
		free(chunk.memory);
		curl_easy_cleanup(curl_handle);
		return NULL;
	}

	curl_easy_cleanup(curl_handle);
	return chunk.memory;
}

static ol_stack *build_thread_index() {
	ol_stack *images_to_download = NULL;

	/* This is where we'll queue up images to be downloaded. */
	images_to_download = malloc(sizeof(ol_stack));
	images_to_download->next = NULL;
	images_to_download->data = NULL;

	unsigned int i;
	for (i = 0; i < (sizeof(BOARDS)/sizeof(BOARDS[0])); i++) {
		const char *current_board = BOARDS[i];

		char buf[128] = {0};
		snprintf(buf, sizeof(buf), "http://a.4cdn.org/%s/catalog.json", current_board);
		char *all_json = get_json(buf);
		if (all_json == NULL) {
			m38_log_msg(LOG_WARN, "Could not receive HTTP from board for /%s/.", current_board);
			free(all_json);
			continue;
		}

		ol_stack *matches = parse_catalog_json(all_json, current_board);

		while (matches->next != NULL) {
			/* Pop our thread_match off the stack */
			thread_match *match = (thread_match*) spop(&matches);
			ensure_directory_for_board(match->board);

			m38_log_msg(LOG_INFO, "/%s/ - Requesting thread %i...", current_board, match->thread_num);

			char templated_req[128] = {0};

			snprintf(templated_req, sizeof(templated_req), "http://a.4cdn.org/%s/thread/%"PRIu64".json",
					match->board, match->thread_num);

			char *thread_json = get_json(templated_req);
			if (thread_json == NULL) {
				m38_log_msg(LOG_WARN, "Could not receive chunked HTTP for thread. continuing.");
				free(match);
				continue;
			}

			ol_stack *thread_matches = parse_thread_json(thread_json, match);
			while (thread_matches->next != NULL) {
				post_match *p_match = (post_match *)spop(&thread_matches);

				char fname[MAX_IMAGE_FILENAME_SIZE] = {0};
				int should_skip = get_non_colliding_image_file_path(fname, p_match);

				/* We already have that file. */
				if (should_skip) {
					free(p_match->body_content);
					free(p_match);
					continue;
				}

				/* Check if we have an existing alias for this file. */
				char key[MAX_KEY_SIZE] = {0};
				webm_alias *existing = get_aliased_image_by_oleg_key(fname, key);
				if (existing) {
					m38_log_msg(LOG_INFO, "Found alias for '%s', skipping.", fname);
					free(p_match->body_content);
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

	return images_to_download;
}

int download_image(const post_match *p_match, const unsigned int post_id) {
	int thumb_request_fd = 0;
	int image_request_fd = 0;
	unsigned char *raw_thumb_resp = NULL;
	unsigned char *raw_image_resp = NULL;
	FILE *thumb_file = NULL;
	FILE *image_file = NULL;

	if (!p_match->should_download_image)
		goto end;

	thumb_request_fd = connect_to_host(FOURCHAN_THUMBNAIL_HOST);
	if (thumb_request_fd < 0) {
		m38_log_msg(LOG_ERR, "Could not connect to thumbnail host.");
		goto error;
	}

	image_request_fd = connect_to_host(FOURCHAN_IMAGE_HOST);
	if (image_request_fd < 0) {
		m38_log_msg(LOG_ERR, "Could not connect to image host.");
		goto error;
	}

	char image_filename[MAX_IMAGE_FILENAME_SIZE] = {0};
	int should_skip = get_non_colliding_image_file_path(image_filename, p_match);

	/* We already have that file. */
	if (should_skip) {
		close(image_request_fd);
		close(thumb_request_fd);
		return 1;
	}

	char thumb_filename[MAX_IMAGE_FILENAME_SIZE] = {0};
	ensure_thumb_directory(p_match);
	get_thumb_filename(thumb_filename, p_match);

	m38_log_msg(LOG_INFO, "Downloading %s%.*s...", p_match->filename, 5, p_match->file_ext);

	/* Build and send the thumbnail request. */
	char thumb_request[256] = {0};
	snprintf(thumb_request, sizeof(thumb_request), THUMB_REQUEST,
			p_match->board, p_match->post_date);
	unsigned int rc = send(thumb_request_fd, thumb_request, strlen(thumb_request), 0);
	if (rc != strlen(thumb_request)) {
		m38_log_msg(LOG_ERR, "Could not send all bytes to host while requesting thumbnail. Sent (%i/%i).", rc, strlen(thumb_request));
		goto error;
	}

	/* Build and send the image request. */
	char image_request[256] = {0};
	snprintf(image_request, sizeof(image_request), IMAGE_REQUEST,
			p_match->board, p_match->post_date, (int)sizeof(p_match->file_ext), p_match->file_ext);
	rc = send(image_request_fd, image_request, strlen(image_request), 0);
	if (rc != strlen(image_request)) {
		m38_log_msg(LOG_ERR, "Could not send all bytes to host while requesting image.");
		goto error;
	}

	size_t thumb_size = 0, image_size = 0;
	raw_thumb_resp = receive_http(thumb_request_fd, &thumb_size);
	raw_image_resp = receive_http(image_request_fd, &image_size);

	if (thumb_size <= 0 || image_size <= 0) {
		close(image_request_fd);
		close(thumb_request_fd);
		free(raw_thumb_resp);
		free(raw_image_resp);
		m38_log_msg(LOG_ERR, "Thumb or image size was zero bytes.");
		return 0;
	}

	if (raw_thumb_resp == NULL) {
		m38_log_msg(LOG_ERR, "No thumbnail received.");
		goto error;
	}

	if (raw_image_resp == NULL) {
		m38_log_msg(LOG_ERR, "No image received.");
		goto error;
	}

	/* Write thumbnail to disk. */
	thumb_file = fopen(thumb_filename, "wb");
	if (!thumb_file || ferror(thumb_file)) {
		m38_log_msg(LOG_ERR, "Could not open thumbnail file: %s", thumb_filename);
		perror(NULL);
		goto error;
	}
	const size_t rt_written = fwrite(raw_thumb_resp, 1, thumb_size, thumb_file);
	m38_log_msg(LOG_INFO, "Wrote %i bytes of thumbnail to disk.", rt_written);
	if (rt_written <= 0 || ferror(thumb_file)) {
		m38_log_msg(LOG_WARN, "Could not write thumbnail to disk: %s", thumb_filename);
		perror(NULL);
		goto error;
	}
	fclose(thumb_file);
	thumb_file = 0;

	image_file = fopen(image_filename, "wb");
	if (!image_file || ferror(image_file)) {
		m38_log_msg(LOG_ERR, "Could not open image file: %s", image_filename);
		perror(NULL);
		goto error;
	}
	const size_t iwritten = fwrite(raw_image_resp, 1, image_size, image_file);
	if (iwritten <= 0 || ferror(image_file)) {
		m38_log_msg(LOG_WARN, "Could not write image to disk: %s", image_filename);
		perror(NULL);
		goto error;
	}
	m38_log_msg(LOG_INFO, "Wrote %i bytes of image to disk.", iwritten);
	fclose(image_file);
	image_file = 0;

	char fname_plus_extension[MAX_IMAGE_FILENAME_SIZE] = {0};
	get_non_colliding_image_filename(fname_plus_extension, p_match);

	/* image_filename is the full path, fname_plus_extension is the file name. */
	int added = add_image_to_db(image_filename, fname_plus_extension, p_match->board, post_id);
	if (!added) {
		m38_log_msg(LOG_WARN, "Could not add image to database. Continuing...");
	}

	/* Don't need the post match anymore: */
	free(raw_image_resp);
	raw_image_resp = NULL;
	free(raw_thumb_resp);
	raw_thumb_resp = NULL;

	close(thumb_request_fd);
	close(image_request_fd);

	m38_log_msg(LOG_INFO, "Downloaded %s%.*s...", p_match->filename, 5, p_match->file_ext);

end:
	return 1;

error:
	free(raw_thumb_resp);
	free(raw_image_resp);
	if (thumb_request_fd)
		close(thumb_request_fd);

	if (image_request_fd)
		close(image_request_fd);

	if (thumb_file != NULL)
		fclose(thumb_file);

	if (image_file != NULL)
		fclose(image_file);
	return 0;
}

int download_images() {
	struct stat st = {0};
	if (stat(webm_location(), &st) == -1) {
		m38_log_msg(LOG_WARN, "Creating webms directory %s.", webm_location());
		mkdir(webm_location(), 0755);
	}

	ol_stack *images_to_download = NULL;
	images_to_download = build_thread_index();
	if (images_to_download == NULL) {
		m38_log_msg(LOG_WARN, "No images to download.");
		return -1;
	}

	/* Now actually download the images. */
	while (images_to_download->next != NULL) {
		post_match *p_match = (post_match *)spop(&images_to_download);

		unsigned int post_id = add_post_to_db(p_match);
		if (!post_id)
			m38_log_msg(LOG_ERR, "Could not add post %s to database.", p_match->post_date);

		if (!download_image(p_match, post_id))
			m38_log_msg(LOG_ERR, "Could not download image.");


		free(p_match->body_content);
		free(p_match);
	}

	free(images_to_download);
	m38_log_msg(LOG_INFO, "Downloaded all images.");

	return 0;
}


int main(int argc, char *argv[]) {
	UNUSED(argc);
	UNUSED(argv);

	m38_log_msg(LOG_INFO, "Downloader started.");
	while (1) {
		if (download_images() != 0) {
			m38_log_msg(LOG_WARN, "Something went wrong while downloading images.");
		}
		sleep(1200); /* 20 Minutes */
	}

	return 0;
}
