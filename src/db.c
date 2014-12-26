// vim: noet ts=4 sw=4
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>

#include "db.h"
#include "http.h"
#include "logging.h"
#include "models.h"
#include "sha3api_ref.h"
#include "utils.h"

static const char db_request[] = "GET /waifu/%s HTTP/1.1\r\n\r\n";
/*
static const char db_post[] = "POST /waifu/%s HTTP/1.1\r\n"
	"Content-Length: %zu\r\n"
	"\r\n"
	"%s";
	*/

int hash_image(const char *file_path, char outbuf[static HASH_IMAGE_STR_SIZE]) {
	int fd = open(file_path, O_RDONLY);
	unsigned char *data_ptr = NULL;

	struct stat st = {0};
	if (stat(file_path, &st) == -1) {
		goto error;
	}

	data_ptr = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	unsigned char hash[HASH_ARRAY_SIZE] = {0};

	if (Hash(IMAGE_HASH_SIZE, data_ptr, st.st_size, hash) != 0) {
		goto error;
	}

	int j = 0;
	for (j = 0; j < HASH_ARRAY_SIZE; j++)
		sprintf(outbuf + (j * 2), "%02X", hash[j]);
	munmap(data_ptr, st.st_size);
	close(fd);

	return 1;
error:
	if (data_ptr != NULL)
		munmap(data_ptr, st.st_size);
	close(fd);
	return 0;
}

unsigned char *fetch_data_from_db(const char key[static MAX_KEY_SIZE], size_t *outdata) {
	unsigned char *_data = NULL;

	int sock = 0;
	sock = connect_to_host_with_port(DB_HOST, DB_PORT);
	assert(sock != 0);

	const size_t db_request_siz = strlen(db_request) + strnlen(key, MAX_KEY_SIZE);
	char new_db_request[db_request_siz];
	memset(new_db_request, '\0', db_request_siz);

	snprintf(new_db_request, db_request_siz, db_request, key);
	int rc = send(sock, new_db_request, strlen(new_db_request), 0);
	if (strlen(new_db_request) != rc)
		goto error;

	_data = receive_http(sock, outdata);
	if (!_data)
		goto error;

	log_msg(LOG_INFO, "Received: %s", _data);

	close(sock);
	return _data;

error:
	free(_data);
	close(sock);
	return NULL;
}

int store_data_in_db(const char key[static MAX_KEY_SIZE], const void *val, const size_t vlen) {
	return 0;
}

webm *get_image(const char image_hash[static HASH_ARRAY_SIZE]) {
	char key[MAX_KEY_SIZE] = {0};
	create_webm_key(image_hash, key);

	size_t json_size = 0;
	char *json = (char *)fetch_data_from_db(key, &json_size);
	log_msg(LOG_INFO, "Json from DB: %s", json);

	if (json == NULL)
		return NULL;

	return deserialize_webm(json);
}

int set_image(const webm *webm) {
	char key[MAX_KEY_SIZE] = {0};
	create_webm_key(webm->file_hash, key);

	char *serialized = serialize_webm(webm);
	log_msg(LOG_INFO, "Serialized: %s", serialized);

	int ret = store_data_in_db(key, serialized, strlen(serialized));
	free(serialized);

	return ret;
}

static int _insert_webm(const char *file_path, const char image_hash[static HASH_IMAGE_STR_SIZE], const char board[static MAX_BOARD_NAME_SIZE]) {
	time_t modified_time = get_file_creation_date(file_path);
	if (modified_time == 0) {
		log_msg(LOG_ERR, "File does not exist.");
		return 0;
	}

	size_t size = get_file_size(file_path);
	if (size == 0) {
		log_msg(LOG_ERR, "File does not exist.");
		return 0;
	}

	webm to_insert = {
		.file_hash = {0},
		.filename = {0},
		.board = {0},
		.created_at = modified_time,
		.size = size
	};
	memcpy(to_insert.file_hash, image_hash, sizeof(to_insert.file_hash));
	memcpy(to_insert.filename, file_path, sizeof(to_insert.filename));
	memcpy(to_insert.board, board, sizeof(to_insert.board));

	return set_image(&to_insert);
}

static int _insert_aliased_webm(const char *file_path, const char image_hash[static HASH_IMAGE_STR_SIZE], const char board[static MAX_BOARD_NAME_SIZE]) {
	return 0;
}

int add_image_to_db(const char *file_path, const char board[MAX_BOARD_NAME_SIZE]) {
	char image_hash[HASH_IMAGE_STR_SIZE] = {0};
	if (!hash_image(file_path, image_hash))
		return 0;

	webm *_old_webm = get_image(image_hash);

	if (_old_webm == NULL)
		return _insert_webm(file_path, image_hash, board);

	/* We don't actually need it... */
	free(_old_webm);

	return _insert_aliased_webm(file_path, image_hash, board);
}
