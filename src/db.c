// vim: noet ts=4 sw=4
#include <stdio.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>

#include "db.h"
#include "http.h"
#include "logging.h"
#include "models.h"
#include "utils.h"

static const char DB_REQUEST[] = "GET /%s/%s HTTP/1.1\r\n"
	"Host: "DB_HOST":"DB_PORT"\r\n"
	"\r\n";

static const char DB_POST[] = "POST /%s/%s HTTP/1.1\r\n"
	"Host: "DB_HOST":"DB_PORT"\r\n"
	"Content-Length: %zu\r\n"
	"Content-Type: application/json\r\n"
	"\r\n"
	"%s";

/* We use 'Accept-Encoding: identity' here so we don't get back chunked
 * transfer shit. I hate parsing that garbage.
 */
static const char DB_MATCH[] =  "GET /%s/%s/_match HTTP/1.1\r\n"
	"Host: "DB_HOST":"DB_PORT"\r\n"
	"Accept-Encoding: identity\r\n"
	"\r\n";

static int _fetch_matches_common(const char prefix[static MAX_KEY_SIZE]) {
	const size_t db_match_siz = strlen(WAIFU_NMSPC) + strlen(DB_MATCH) + strnlen(prefix, MAX_KEY_SIZE);
	char new_db_request[db_match_siz];
	memset(new_db_request, '\0', db_match_siz);

	int sock = 0;
	sock = connect_to_host_with_port(DB_HOST, DB_PORT);
	if (sock == 0)
		goto error;

	snprintf(new_db_request, db_match_siz, DB_MATCH, WAIFU_NMSPC, prefix);
	int rc = send(sock, new_db_request, strlen(new_db_request), 0);
	if (strlen(new_db_request) != rc)
		goto error;

	return sock;

error:
	close(sock);
	return 0;
}

unsigned int fetch_num_matches_from_db(const char prefix[static MAX_KEY_SIZE]) {
	size_t outdata = 0;
	char *_data = NULL;
	char *_value = NULL;

	int sock = _fetch_matches_common(prefix);
	if (!sock)
		goto error;

	_data = receieve_only_http_header(sock, SELECT_TIMEOUT, &outdata);
	if (!_data)
		goto error;

	_value = get_header_value(_data, outdata, "X-Olegdb-Num-Matches");
	if (!_value)
		goto error;

	unsigned int to_return = strtol(_value, NULL, 10);

	free(_value);
	close(sock);
	return to_return;

error:
	free(_value);
	free(_data);
	close(sock);
	return 0;
}

db_key_match *fetch_matches_from_db(const char prefix[static MAX_KEY_SIZE]) {
	size_t dsize = 0;
	unsigned char *_data = NULL;

	int sock = _fetch_matches_common(prefix);
	if (!sock)
		goto error;

	_data = receive_http(sock, &dsize);
	if (!_data)
		goto error;

	db_key_match *eol = NULL;
	db_key_match *cur = eol;
	int i;
	unsigned char *line_start = _data, *line_end = NULL;
	for (i = 0; i < dsize; i++) {
		if (_data[i] == '\n' && i + 1 < dsize) {
			line_end = &_data[i];
			const size_t line_size = line_end - line_start;

			db_key_match *new = calloc(1, sizeof(db_key_match));
			memcpy(new->key, line_start, line_size);

			new->next = cur;
			cur = new;
			line_start = &_data[++i];
		}
	}

	free(_data);
	close(sock);
	return cur;

error:
	free(_data);
	close(sock);
	return NULL;
}

unsigned char *fetch_data_from_db(const char key[static MAX_KEY_SIZE], size_t *outdata) {
	unsigned char *_data = NULL;

	const size_t db_request_siz = strlen(WAIFU_NMSPC) + strlen(DB_REQUEST) + strnlen(key, MAX_KEY_SIZE);
	char new_db_request[db_request_siz];
	memset(new_db_request, '\0', db_request_siz);

	int sock = 0;
	sock = connect_to_host_with_port(DB_HOST, DB_PORT);
	if (sock == 0)
		goto error;

	snprintf(new_db_request, db_request_siz, DB_REQUEST, WAIFU_NMSPC, key);
	int rc = send(sock, new_db_request, strlen(new_db_request), 0);
	if (strlen(new_db_request) != rc)
		goto error;

	_data = receive_http(sock, outdata);
	if (!_data)
		goto error;

	/* log_msg(LOG_INFO, "Received: %s", _data); */

	close(sock);
	return _data;

error:
	free(_data);
	close(sock);
	return NULL;
}

int store_data_in_db(const char key[static MAX_KEY_SIZE], const unsigned char *val, const size_t vlen) {
	unsigned char *_data = NULL;

	const size_t vlen_len = UINT_LEN(vlen);
	/* See DB_POST for why we need all this. */
	const size_t db_post_siz = strlen(WAIFU_NMSPC) + strlen(key) + strlen(DB_POST) + vlen_len + vlen;
	char new_db_post[db_post_siz + 1];
	memset(new_db_post, '\0', db_post_siz + 1);

	int sock = 0;
	sock = connect_to_host_with_port(DB_HOST, DB_PORT);
	if (sock == 0)
		goto error;

	sprintf(new_db_post, DB_POST, WAIFU_NMSPC, key, vlen, val);
	int rc = send(sock, new_db_post, strlen(new_db_post), 0);
	if (strlen(new_db_post) != rc) {
		log_msg(LOG_ERR, "Could not send stuff to DB.");
		goto error;
	}

	/* I don't really care about the reply, but I probably should. */
	size_t out;
	_data = receive_http(sock, &out);
	if (!_data) {
		log_msg(LOG_ERR, "No reply from DB.");
		goto error;
	}

	free(_data);
	close(sock);
	return 1;

error:
	free(_data);
	close(sock);
	return 0;
}

/* Webm get/set stuff */
webm *get_image(const char image_hash[static HASH_ARRAY_SIZE]) {
	char key[MAX_KEY_SIZE] = {0};
	create_webm_key(image_hash, key);

	size_t json_size = 0;
	char *json = (char *)fetch_data_from_db(key, &json_size);
	/* log_msg(LOG_INFO, "Json from DB: %s", json); */

	if (json == NULL)
		return NULL;

	return deserialize_webm(json);
}

int set_image(const webm *webm) {
	char key[MAX_KEY_SIZE] = {0};
	create_webm_key(webm->file_hash, key);

	char *serialized = serialize_webm(webm);
	log_msg(LOG_INFO, "Serialized: %s", serialized);

	int ret = store_data_in_db(key, (unsigned char *)serialized, strlen(serialized));
	free(serialized);

	return ret;
}

webm_alias *get_aliased_image(const char filepath[static MAX_IMAGE_FILENAME_SIZE]) {
	char key[MAX_KEY_SIZE] = {0};
	create_alias_key(filepath, key);

	size_t json_size = 0;
	char *json = (char *)fetch_data_from_db(key, &json_size);

	if (json == NULL)
		return NULL;

	return deserialize_alias(json);
}

/* Alias get/set stuff */
int set_aliased_image(const webm_alias *alias) {
	char key[MAX_KEY_SIZE] = {0};
	create_alias_key(alias->filename, key);

	char *serialized = serialize_alias(alias);
	/* log_msg(LOG_INFO, "Serialized: %s", serialized); */

	int ret = store_data_in_db(key, (unsigned char *)serialized, strlen(serialized));
	free(serialized);

	return ret;
}

static int _insert_webm(const char *file_path, const char filename[static MAX_IMAGE_FILENAME_SIZE], 
						const char image_hash[static HASH_IMAGE_STR_SIZE], const char board[static MAX_BOARD_NAME_SIZE]) {
	time_t modified_time = get_file_creation_date(file_path);
	if (modified_time == 0) {
		log_msg(LOG_ERR, "IWMT: '%s' does not exist.", file_path);
		return 0;
	}

	size_t size = get_file_size(file_path);
	if (size == 0) {
		log_msg(LOG_ERR, "IWFS: '%s' does not exist.", file_path);
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
	memcpy(to_insert.filename, filename, sizeof(to_insert.filename));
	memcpy(to_insert.board, board, sizeof(to_insert.board));

	return set_image(&to_insert);
}

static int _insert_aliased_webm(const char *file_path, const char filename[static MAX_IMAGE_FILENAME_SIZE],
								const char image_hash[static HASH_IMAGE_STR_SIZE], const char board[static MAX_BOARD_NAME_SIZE]) {
	time_t modified_time = get_file_creation_date(file_path);
	if (modified_time == 0) {
		log_msg(LOG_ERR, "IAWMT: '%s' does not exist.", file_path);
		return 0;
	}

	size_t size = get_file_size(file_path);
	if (size == 0) {
		log_msg(LOG_ERR, "IAWFS: '%s' does not exist.", file_path);
		return 0;
	}

	webm_alias to_insert = {
		.file_hash = {0},
		.filename = {0},
		.board = {0},
		.created_at = modified_time,
	};

	memcpy(to_insert.file_hash, image_hash, sizeof(to_insert.file_hash));
	memcpy(to_insert.filename, file_path, sizeof(to_insert.filename));
	memcpy(to_insert.board, board, sizeof(to_insert.board));

	return set_aliased_image(&to_insert);
}

int add_image_to_db(const char *file_path, const char *filename, const char board[MAX_BOARD_NAME_SIZE]) {
	char image_hash[HASH_IMAGE_STR_SIZE] = {0};
	if (!hash_file(file_path, image_hash))
		return 0;

	webm *_old_webm = get_image(image_hash);

	if (!_old_webm)
		return _insert_webm(file_path, filename, image_hash, board);

	/* Check to see if the one we're scanning is the exact match we got from the DB. */
	if (strncmp(_old_webm->filename, filename, MAX_IMAGE_FILENAME_SIZE) != 0) {
		/* It's not the canonical original, so insert an alias. */

		webm_alias *_old_alias = get_aliased_image(file_path);
		int rc = 1;
		/* If we DONT already have an alias for this image with this filename,
		 * insert one.
		 * We know if this filename is an alias already because the hash of
		 * an alias is it's filename.
		 */
		if (_old_alias == NULL) {
			rc = _insert_aliased_webm(file_path, filename, image_hash, board);
			log_msg(LOG_FUN, "%s is a new alias of %s.", file_path, _old_webm->filename);
		} else {
			/* Regardless, this webm is an alias and we don't care. Delete it. */
			/* There are some bad values in the database. Skip them. */
			if (endswith(_old_alias->filename, ".webm"))
				log_msg(LOG_ERR, "'%s' is a bad value.", _old_webm->filename);
			log_msg(LOG_WARN, "%s is already marked as an alias of %s.", file_path, _old_webm->filename);
		}

		if (rc)
			unlink(file_path);
		free(_old_alias);
		free(_old_webm);
		return rc;
	}

	/* The one we got from the DB is the one we're working on, or at least
	 * it has the same name and file hash.
	 */
	free(_old_webm);
	return 1;

}

db_match *filter(const char prefix[static MAX_KEY_SIZE], int (*filter)(unsigned char *data)) {
	db_match *eol = NULL;

	db_key_match *prefix_matches = fetch_matches_from_db(prefix);
	db_key_match *current = prefix_matches;
	while (current) {
		db_key_match *next = current->next;


		/* 1. Fetch data for this key from DB.
		 * 2. Apply filter predicate.
		 * 3. If it returns true, add it to the list.
		 * 4. Continue.
		 */

		free(current);
		current = next;
	}

	return eol;
}
