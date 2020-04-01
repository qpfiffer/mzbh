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
#include "stack.h"
#include "utils.h"

const char *BOARDS[] = {"a", "b", "fit", "g", "gif", "e", "h", "o", "n", "r", "s", "sci", "soc", "v", "wsg"};

struct MemoryStruct {
	char *memory;
	size_t size;
};

static size_t write_file_callback(void *contents, size_t size,
								  size_t nmemb, void *userp) {
	return fwrite(contents, size, nmemb, (FILE *)userp);
}

static size_t write_memory_callback(void *contents, size_t size,
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

static int get_file(const char *url, FILE *out_file) {
	CURL *curl_handle;
	CURLcode res;

	curl_handle = curl_easy_init();
	curl_easy_setopt(curl_handle, CURLOPT_URL, url);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_file_callback);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)out_file);
	curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
	res = curl_easy_perform(curl_handle);

	if (res != CURLE_OK) {
		const char *err = curl_easy_strerror(res);
		m38_log_msg(LOG_WARN, "Could not receive chunked HTTP from board: %s", err);
		curl_easy_cleanup(curl_handle);
		return 1;
	}

	curl_easy_cleanup(curl_handle);
	return 0;
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
	FILE *thumb_file = NULL;
	FILE *image_file = NULL;

	if (!p_match->should_download_image)
		goto end;

	char image_filename[MAX_IMAGE_FILENAME_SIZE] = {0};
	int should_skip = get_non_colliding_image_file_path(image_filename, p_match);

	/* We already have that file. */
	if (should_skip)
		return 1;

	char thumb_filename[MAX_IMAGE_FILENAME_SIZE] = {0};
	ensure_thumb_directory(p_match);
	get_thumb_filename(thumb_filename, p_match);

	m38_log_msg(LOG_INFO, "Downloading %s%.*s...", p_match->filename, 5, p_match->file_ext);

	thumb_file = fopen(thumb_filename, "wb");
	if (!thumb_file || ferror(thumb_file)) {
		m38_log_msg(LOG_ERR, "Could not open thumbnail file: %s", thumb_filename);
		perror(NULL);
		goto error;
	}

	/* Build and send the thumbnail request. */
	char templated_req[512] = {0};
	snprintf(templated_req, sizeof(templated_req), "https://t.4cdn.org/%s/%ss.jpg",
			p_match->board, p_match->post_date);
	int rc = get_file(templated_req, thumb_file);
	fclose(thumb_file);

	if (rc) {
		m38_log_msg(LOG_ERR, "Could not write thumbnail file.");
		goto error;
	}

	image_file = fopen(image_filename, "wb");
	if (!image_file || ferror(image_file)) {
		m38_log_msg(LOG_ERR, "Could not open image file: %s", image_filename);
		perror(NULL);
		goto error;
	}

	/* Build and send the image request. */
	char image_request[512] = {0};
	snprintf(image_request, sizeof(image_request), "https://i.4cdn.org/%s/%s%.*s",
			p_match->board, p_match->post_date, (int)sizeof(p_match->file_ext), p_match->file_ext);
	rc = get_file(image_request, image_file);
	fclose(image_file);

	if (rc) {
		m38_log_msg(LOG_ERR, "Could not write image file.");
		goto error;
	}

	char fname_plus_extension[MAX_IMAGE_FILENAME_SIZE] = {0};
	get_non_colliding_image_filename(fname_plus_extension, p_match);

	/* image_filename is the full path, fname_plus_extension is the file name. */
	int added = add_image_to_db(image_filename, fname_plus_extension, p_match->board, post_id);
	if (!added) {
		m38_log_msg(LOG_WARN, "Could not add image to database. Continuing...");
	}

	/* Don't need the post match anymore: */

	m38_log_msg(LOG_INFO, "Downloaded %s%.*s...", p_match->filename, 5, p_match->file_ext);

end:
	return 1;

error:
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
