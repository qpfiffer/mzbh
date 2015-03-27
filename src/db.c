// vim: noet ts=4 sw=4
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>

#include "db.h"
#include "benchmark.h"
#include "http.h"
#include "logging.h"
#include "models.h"
#include "utils.h"

static const char DB_REQUEST[] = "GET /%s/%s HTTP/1.1\r\n"
	"Host: "DB_HOST":"DB_PORT"\r\n"
	"Accept-Encoding: identity\r\n"
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

static const char DB_BULK_UNJAR[] =  "POST /%s/_bulk_unjar HTTP/1.1\r\n"
	"Host: "DB_HOST":"DB_PORT"\r\n"
	"Content-Length: %zu\r\n"
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
	unsigned int rc = send(sock, new_db_request, strlen(new_db_request), 0);
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

	const struct bmark x = begin_benchmark("fetch_num_matches_from_db");
	int sock = _fetch_matches_common(prefix);
	if (!sock)
		goto error;

	_data = receive_only_http_header(sock, SELECT_TIMEOUT, &outdata);
	if (!_data)
		goto error;

	_value = get_header_value(_data, outdata, "X-Olegdb-Num-Matches");
	if (!_value)
		goto error;

	end_benchmark(x);
	unsigned int to_return = strtol(_value, NULL, 10);

	free(_data);
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
	unsigned int i;
	unsigned char *line_start = _data, *line_end = NULL;
	for (i = 0; i < dsize; i++) {
		if (_data[i] == '\n' || i == (dsize - 1)) {
			if (i == (dsize - 1))
				line_end = &_data[i + 1];
			else
				line_end = &_data[i];
			const size_t line_size = line_end - line_start;

			db_key_match _stack = {
				.key = {0},
				.next = cur
			};
			memcpy((char *)_stack.key, line_start, line_size);

			db_key_match *new = calloc(1, sizeof(db_key_match));
			memcpy(new, &_stack, sizeof(db_key_match));

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


	const struct bmark x = begin_benchmark("fetch_data_from_db");
	int sock = 0;
	sock = connect_to_host_with_port(DB_HOST, DB_PORT);
	if (sock == 0)
		goto error;

	snprintf(new_db_request, db_request_siz, DB_REQUEST, WAIFU_NMSPC, key);
	unsigned int rc = send(sock, new_db_request, strlen(new_db_request), 0);
	if (strlen(new_db_request) != rc)
		goto error;

	_data = receive_http(sock, outdata);
	end_benchmark(x);
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
	int attempts = 1;
	unsigned char *_data = NULL;
	int sock = 0;

	const int max_attempts = 3;
	while (attempts <= max_attempts) {
		const size_t vlen_len = UINT_LEN(vlen);
		/* See DB_POST for why we need all this. */
		const size_t db_post_siz = strlen(WAIFU_NMSPC) + strlen(key) + strlen(DB_POST) + vlen_len + vlen;
		char new_db_post[db_post_siz + 1];
		memset(new_db_post, '\0', db_post_siz + 1);

		const struct bmark x = begin_benchmark("store_data_in_db");
		sock = 0;
		sock = connect_to_host_with_port(DB_HOST, DB_PORT);
		if (sock == 0) {
			log_msg(LOG_ERR, "(%i/%i): Could not connect to host.", attempts, max_attempts);
			attempts++;
			continue;
		}

		sprintf(new_db_post, DB_POST, WAIFU_NMSPC, key, vlen, val);
		unsigned int rc = send(sock, new_db_post, strlen(new_db_post), 0);
		if (strlen(new_db_post) != rc) {
			log_msg(LOG_ERR, "(%i/%i): Could not send stuff to DB.", attempts, max_attempts);
			attempts++;
			close(sock);
			continue;
		}

		/* I don't really care about the reply, but I probably should. */
		size_t out;
		_data = receive_http(sock, &out);
		end_benchmark(x);
		if (!_data) {
			log_msg(LOG_ERR, "(%i/%i): Store data: No reply from DB.", attempts, max_attempts);
			attempts++;
			close(sock);
			continue;
		}

		free(_data);
		close(sock);
		return 1;
	}

	free(_data);
	close(sock);
	log_msg(LOG_ERR, "Could not store data in DB.");
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

	webm *deserialized = deserialize_webm(json);
	free(json);
	return deserialized;
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

webm_alias *get_aliased_image_with_key(const char key[static MAX_KEY_SIZE]) {
	size_t json_size = 0;
	char *json = (char *)fetch_data_from_db(key, &json_size);

	if (json == NULL)
		return NULL;

	webm_alias *alias = deserialize_alias(json);
	free(json);
	return alias;
}

webm_alias *get_aliased_image(const char filepath[static MAX_IMAGE_FILENAME_SIZE]) {
	char key[MAX_KEY_SIZE] = {0};
	create_alias_key(filepath, key);

	return get_aliased_image_with_key(key);
}

webm_to_alias *get_webm_to_alias(const char image_hash[static HASH_ARRAY_SIZE]) {
	char key[MAX_KEY_SIZE] = {0};
	create_webm_to_alias_key(image_hash, key);

	size_t json_size = 0;
	char *json = (char *)fetch_data_from_db(key, &json_size);

	if (json == NULL)
		return NULL;

	webm_to_alias *w2a = deserialize_webm_to_alias(json);
	free(json);
	return w2a;
}

/* Alias get/set stuff */
int set_aliased_image(const webm_alias *alias) {
	char key[MAX_KEY_SIZE] = {0};
	create_alias_key(alias->file_path, key);

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
	memcpy(to_insert.file_path, file_path, sizeof(to_insert.file_path));
	memcpy(to_insert.filename, filename, sizeof(to_insert.filename));
	memcpy(to_insert.board, board, sizeof(to_insert.board));

	return set_image(&to_insert);
}

static int _insert_aliased_webm(const char *file_path,
								const char *filename,
								const char image_hash[static HASH_IMAGE_STR_SIZE],
								const char board[static MAX_BOARD_NAME_SIZE]) {
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
		.file_path = {0},
		.filename = {0},
		.board = {0},
		.created_at = modified_time,
	};

	memcpy(to_insert.file_hash, image_hash, sizeof(to_insert.file_hash));
	memcpy(to_insert.filename, filename, sizeof(to_insert.filename));
	memcpy(to_insert.file_path, file_path, sizeof(to_insert.file_path));
	memcpy(to_insert.board, board, sizeof(to_insert.board));

	return set_aliased_image(&to_insert);
}

int add_image_to_db(const char *file_path, const char *filename, const char board[MAX_BOARD_NAME_SIZE]) {
	char image_hash[HASH_IMAGE_STR_SIZE] = {0};
	if (!hash_file(file_path, image_hash)) {
		log_msg(LOG_ERR, "Could not hash '%s'.", file_path);
		return 0;
	}

	webm *_old_webm = get_image(image_hash);

	if (!_old_webm) {
		int rc = _insert_webm(file_path, filename, image_hash, board);
		if (!rc)
			log_msg(LOG_ERR, "Something went wrong inserting webm.");
		return rc;
	}

	/* Check to see if the one we're scanning is the exact match we got from the DB. */
	if (strncmp(_old_webm->filename, filename, MAX_IMAGE_FILENAME_SIZE) == 0 &&
		strncmp(_old_webm->board, board, MAX_BOARD_NAME_SIZE) == 0) {
		/* The one we got from the DB is the one we're working on, or at least
		 * it has the same name, board and file hash.
		 */
		free(_old_webm);
		return 1;
	}

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
		log_msg(LOG_FUN, "%s (%s) is a new alias of %s (%s).", filename, board, _old_webm->filename, _old_webm->board);
	} else {
		/* Regardless, this webm is an alias and we don't care. Delete it. */
		/* There are some bad values in the database. Skip them. */
		if (!endswith(_old_alias->filename, ".webm"))
			log_msg(LOG_ERR, "'%s' is a bad value.", _old_webm->filename);
		log_msg(LOG_WARN, "%s is already marked as an alias of %s. Old alias is: '%s'",
				file_path, _old_webm->filename, _old_alias->filename);
	}

	char alias_key[MAX_KEY_SIZE] = {0};
	create_alias_key(file_path, alias_key);
	/* Create or update the many2many between these two: */
	associate_alias_with_webm(_old_webm, alias_key);

	if (rc) {
		log_msg(LOG_WARN, "Unlinking '%s'.", file_path);
		//unlink(file_path);
	} else
		log_msg(LOG_ERR, "Something went wrong when adding image to db.");
	free(_old_alias);
	free(_old_webm);
	return rc;
}

static inline size_t _nline_delimited_key_size(const struct db_key_match *keys) {
	size_t siz = 0;

	const db_key_match *current = keys;
	while (current) {
		siz += strlen(current->key);
		current = current->next;
		if (current)
			siz += strlen("\n");
	}

	return siz;
}

static inline db_match *_parse_bulk_response(const unsigned char *data, const size_t dsize) {
	db_match *last = NULL;
	db_match *matches = NULL;

	unsigned int i = 0;

	while (i < dsize) {
		char siz_buf[BUNJAR_SIZE_SIZ + 1] = {0};
		unsigned char *data_buf = NULL;

		/* Read in size first */
		unsigned int j;
		for (j = 0; j < BUNJAR_SIZE_SIZ; j++)
			siz_buf[j] = data[j + i];

		i += j;

		const long long record_size = strtol(siz_buf, NULL, 10);
		if ((record_size == LONG_MIN || record_size == LONG_MAX) && errno == ERANGE) {
			log_msg(LOG_ERR, "Could not parse out chunk size.");
			goto error;
		}

		data_buf = malloc(record_size + 1);
		if (!data_buf) {
			log_msg(LOG_ERR, "Could not malloc buffer for record.");
			goto error;
		}
		data_buf[record_size] = '\0';

		if (record_size == 0)
			continue;

		for (j = 0; j < record_size; j++) {
			data_buf[j] = data[i + j];
		}

		db_match *actual = malloc(sizeof(db_match));
		if (!actual) {
			log_msg(LOG_ERR, "Could not malloc buffer for match.");
			goto error;
		}

		db_match _tmp = {
			.data = data_buf,
			.dsize = record_size,
			.extradata = NULL,
			.next = NULL
		};
		memcpy(actual, &_tmp, sizeof(db_match));

		/* This is dumb but I'm on the train right now and I don't care. */
		if (last)
			last->next = actual;
		if (!matches)
			matches = actual;
		last = actual;
		i += j;
	}

	return matches;

error:
	while (matches) {
		db_match *next = matches->next;
		free((unsigned char *)matches->data);
		free(matches);
		matches = next;
	}

	return NULL;
}

db_match *fetch_bulk_from_db(struct db_key_match *keys, int free_keys) {
	size_t dsize = 0;
	unsigned char *_data = NULL;
	int sock = 0;

	const size_t keys_size = _nline_delimited_key_size(keys);
	const size_t db_bu_siz = strlen(WAIFU_NMSPC) + strlen(DB_BULK_UNJAR);
	const size_t total_size = keys_size + db_bu_siz;
	char *new_db_request = malloc(total_size + 1);;
	new_db_request[total_size] = '\0';

	sock = 0;
	sock = connect_to_host_with_port(DB_HOST, DB_PORT);
	if (sock == 0) {
		log_msg(LOG_ERR, "bulk_unjar: Could not connect to OlegDB.");
		goto error;
	}

	/* TODO: Is it faster here to A) Loop through the keys twice, allocating a single
	 * buffer once the size is known or B) looping through the keys once, allocating a
	 * buffer on the stack and resizing it as we go?
	 */
	snprintf(new_db_request, db_bu_siz, DB_BULK_UNJAR, WAIFU_NMSPC, keys_size);
	db_key_match *current = keys;

	while (current) {
		strncat(new_db_request, current->key, total_size);

		current = current->next;
		if (current)
			strncat(new_db_request, "\n", total_size);

		if (free_keys)
			free(current);
	}

	unsigned int rc = send(sock, new_db_request, total_size, 0);
	if (total_size != rc) {
		log_msg(LOG_ERR, "bulk_unjar: Could not send HTTP header to OlegDB.");
		goto error;
	}


	/* Now we get back our weird Oleg-only format of keys. Hopefully
	 * not chunked.
	 */
	_data = receive_http(sock, &dsize);
	if (!_data) {
		log_msg(LOG_ERR, "bulk_unjar: Did not receive response from OlegDB.");
		goto error;
	}

	return _parse_bulk_response(_data, dsize);

error:
	if (sock)
		close(sock);
	free(_data);
	return NULL;
}

db_match *filter(const char prefix[static MAX_KEY_SIZE], const void *extrainput,
		int (*filter)(const unsigned char *data, const size_t dsize, const void *extrainput, void **extradata)) {
	db_match *eol = NULL;
	db_match *cur = eol;

	db_key_match *prefix_matches = fetch_matches_from_db(prefix);
	db_key_match *cur_km = prefix_matches;
	while (cur_km) {
		db_key_match *next = cur_km->next;

		/* 1. Fetch data for this key from DB. */
		size_t dsize = 0;
		unsigned char *_data = fetch_data_from_db(cur_km->key, &dsize);
		/* 2. Apply filter predicate. */
		if (_data) {
			/* 3. If it returns true, add it to the list.*/
			void *extradata = NULL;
			if (filter(_data, dsize, extrainput, &extradata)) {
				db_match _new = {
					.data = _data,
					.dsize = dsize,
					.extradata = extradata,
					.next = cur
				};

				db_match *new = calloc(1, sizeof(db_match));
				memcpy(new, &_new, sizeof(db_match));
				cur = new;
			} else {
				free((unsigned char *)_data);
			}
		}
		/* 4. Continue.*/
		free(cur_km);
		cur_km = next;
	}

	return cur;
}

int associate_alias_with_webm(const webm *webm, const char alias_key[static MAX_KEY_SIZE]) {
	if (!webm || strlen(alias_key) == 0)
		return 0;

	char key[MAX_KEY_SIZE] = {0};
	create_webm_to_alias_key(webm->file_hash, key);

	size_t json_size = 0;
	char *w2a_json = (char *)fetch_data_from_db(key, &json_size);
	if (!w2a_json) {
		/* No existing m2m relation. */
		vector *aliases = vector_new(MAX_KEY_SIZE, 1);
		vector_append(aliases, alias_key, strlen(alias_key));

		webm_to_alias _new_m2m = {
			.aliases = aliases
		};

		const char *serialized = serialize_webm_to_alias(&_new_m2m);
		if (!serialized) {
			log_msg(LOG_ERR, "Could not serialize new webm_to_alias.");
			vector_free(aliases);
			return 0;
		}

		if (!store_data_in_db(key, (unsigned char *)serialized, strlen(serialized))) {
			log_msg(LOG_ERR, "Could not store new webm_to_alias.");
			free((char *)serialized);
			vector_free(aliases);
			return 0;
		}

		vector_free(aliases);
		free((char *)serialized);
		return 1;

	}

	webm_to_alias *deserialized = deserialize_webm_to_alias(w2a_json);
	free(w2a_json);

	if (!deserialized) {
		log_msg(LOG_ERR, "Could not deserialize webm_to_alias.");
		return 0;
	}

	unsigned int i;
	int found = 0;
	for (i = 0; i < deserialized->aliases->count; i++) {
		const char *existing = vector_get(deserialized->aliases, i);
		if (strncmp(existing, alias_key, MAX_KEY_SIZE) == 0) {
			found = 1;
			break;
		}
	}

	/* We found this alias key in the list of them. Skip it. */
	if (found) {
		vector_free(deserialized->aliases);
		free(deserialized);
		return 1;
	}

	vector_append(deserialized->aliases, alias_key, strlen(alias_key));
	char *new_serialized = serialize_webm_to_alias(deserialized);
	vector_free(deserialized->aliases);
	free(deserialized);

	if (!new_serialized) {
		log_msg(LOG_ERR, "Could not serialize webm_to_alias.");
		return 0;
	}

	if (!store_data_in_db(key, (unsigned char *)new_serialized, strlen(new_serialized))) {
		log_msg(LOG_ERR, "Could not store updated webm_to_alias.");
		free(new_serialized);
		return 0;
	}

	free(new_serialized);
	return 1;
}
