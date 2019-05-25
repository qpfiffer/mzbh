// vim: noet ts=4 sw=4
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>

#include <38-moths/logging.h>
#include <libpq-fe.h>

#include "db.h"
#include "benchmark.h"
#include "http.h"
#include "models.h"
#include "parse.h"
#include "utils.h"

static PGconn *_get_pg_connection() {
	PGconn *conn = PQconnectdb(DB_PG_CONNECTION_INFO);

	if (PQstatus(conn) != CONNECTION_OK) {
		m38_log_msg(LOG_ERR, "Could not connect to Postgres: %s", PQerrorMessage(conn));
		return NULL;
	}

	return conn;
}

static void _finish_pg_connection(PGconn *conn) {
	if (conn)
		PQfinish(conn);
}

unsigned int get_record_count_in_table(const char *query_command) {
	PGresult *res = NULL;
	PGconn *conn = NULL;
	unsigned int ret = 0;

	conn = _get_pg_connection();
	if (!conn)
		goto error;

	res = PQexecParams(conn,
					  query_command,
					  0,
					  NULL,
					  NULL,
					  NULL,
					  NULL,
					  1);

	if (PQresultStatus(res) != PGRES_TUPLES_OK) {
		m38_log_msg(LOG_ERR, "SELECT failed: %s", PQerrorMessage(conn));
		goto error;
	}

	char *bytes = PQgetvalue(res, 0, 0);
	(void) bytes;
	ret = *((int *)PQgetvalue(res, 0, PQfnumber(res, "count")));

	_finish_pg_connection(conn);

	return ret;

error:
	if (res)
		PQclear(res);
	_finish_pg_connection(conn);
	return 0;
}

/* Webm get/set stuff */
webm *get_image_by_oleg_key(const char image_hash[static HASH_ARRAY_SIZE], char out_key[static MAX_KEY_SIZE]) {
	PGresult *res = NULL;
	PGconn *conn = NULL;

	create_webm_key(image_hash, out_key);
	const char *param_values[] = {image_hash};

	conn = _get_pg_connection();
	if (!conn)
		goto error;

	res = PQexecParams(conn,
					  "SELECT * FROM webms WHERE file_hash = $1",
					  1,
					  NULL,
					  param_values,
					  NULL,
					  NULL,
					  1); /* <-- binary setting */

	if (PQresultStatus(res) != PGRES_TUPLES_OK) {
		m38_log_msg(LOG_ERR, "SELECT failed: %s", PQerrorMessage(conn));
		goto error;
	}

	webm *deserialized = deserialize_webm_from_tuples(res);
	if (!deserialized)
		goto error;

	PQclear(res);
	_finish_pg_connection(conn);

	return deserialized;

error:
	if (res)
		PQclear(res);
	_finish_pg_connection(conn);
	return NULL;
}

int set_image(const webm *webm) {
	char key[MAX_KEY_SIZE] = {0};
	create_webm_key(webm->file_hash, key);

	char *serialized = serialize_webm(webm);
	m38_log_msg(LOG_INFO, "Serialized: %s", serialized);

	int ret = store_data_in_db(&oleg_conn, key, (unsigned char *)serialized, strlen(serialized));
	free(serialized);

	return ret;
}

webm_alias *get_aliased_image_with_key(const char key[static MAX_KEY_SIZE]) {
	size_t json_size = 0;
	char *json = (char *)fetch_data_from_db(&oleg_conn, key, &json_size);

	if (json == NULL)
		return NULL;

	webm_alias *alias = deserialize_alias(json);
	free(json);
	return alias;
}

webm_alias *get_aliased_image(const char filepath[static MAX_IMAGE_FILENAME_SIZE], char out_key[static MAX_KEY_SIZE]) {
	create_alias_key(filepath, out_key);

	return get_aliased_image_with_key(out_key);
}

webm_to_alias *get_webm_to_alias(const char image_hash[static HASH_ARRAY_SIZE]) {
	char key[MAX_KEY_SIZE] = {0};
	create_webm_to_alias_key(image_hash, key);

	size_t json_size = 0;
	char *json = (char *)fetch_data_from_db(&oleg_conn, key, &json_size);

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
	/* m38_log_msg(LOG_INFO, "Serialized: %s", serialized); */

	int ret = store_data_in_db(&oleg_conn, key, (unsigned char *)serialized, strlen(serialized));
	free(serialized);

	return ret;
}

static int _insert_webm(const char *file_path, const char filename[static MAX_IMAGE_FILENAME_SIZE], 
						const char image_hash[static HASH_IMAGE_STR_SIZE], const char board[static MAX_BOARD_NAME_SIZE],
						const char post_key[MAX_KEY_SIZE]) {
	time_t modified_time = get_file_creation_date(file_path);
	if (modified_time == 0) {
		m38_log_msg(LOG_ERR, "IWMT: '%s' does not exist.", file_path);
		return 0;
	}

	size_t size = get_file_size(file_path);
	if (size == 0) {
		m38_log_msg(LOG_ERR, "IWFS: '%s' does not exist.", file_path);
		return 0;
	}

	webm to_insert = {
		.file_hash = {0},
		.filename = {0},
		.board = {0},
		.created_at = modified_time,
		.size = size,
		.post = {0}
	};
	memcpy(to_insert.file_hash, image_hash, sizeof(to_insert.file_hash));
	memcpy(to_insert.file_path, file_path, sizeof(to_insert.file_path));
	memcpy(to_insert.filename, filename, sizeof(to_insert.filename));
	memcpy(to_insert.board, board, sizeof(to_insert.board));
	memcpy(to_insert.post, post_key, sizeof(to_insert.post));

	return set_image(&to_insert);
}

static int _insert_aliased_webm(const char *file_path,
								const char *filename,
								const char image_hash[static HASH_IMAGE_STR_SIZE],
								const char board[static MAX_BOARD_NAME_SIZE],
								const char post_key[MAX_KEY_SIZE]) {
	time_t modified_time = get_file_creation_date(file_path);
	if (modified_time == 0) {
		m38_log_msg(LOG_ERR, "IAWMT: '%s' does not exist.", file_path);
		return 0;
	}

	size_t size = get_file_size(file_path);
	if (size == 0) {
		m38_log_msg(LOG_ERR, "IAWFS: '%s' does not exist.", file_path);
		return 0;
	}

	webm_alias to_insert = {
		.file_hash = {0},
		.file_path = {0},
		.filename = {0},
		.board = {0},
		.created_at = modified_time,
		.post = {0}
	};

	memcpy(to_insert.file_hash, image_hash, sizeof(to_insert.file_hash));
	memcpy(to_insert.filename, filename, sizeof(to_insert.filename));
	memcpy(to_insert.file_path, file_path, sizeof(to_insert.file_path));
	memcpy(to_insert.board, board, sizeof(to_insert.board));
	memcpy(to_insert.post, post_key, sizeof(to_insert.post));

	return set_aliased_image(&to_insert);
}

void modify_aliased_file(const char *file_path, const webm *_old_webm, const time_t new_stamp) {
	char *real_fpath = NULL, *real_old_fpath = NULL;
	real_fpath = realpath(file_path, NULL);
	real_old_fpath = realpath(_old_webm->file_path, NULL);

	if (!real_fpath || !real_old_fpath) {
		m38_log_msg(LOG_WARN, "One or both files to be linked does not exist.");
		return;
	}

	const size_t bigger = strlen(real_fpath) > strlen(real_old_fpath) ?
			strlen(real_fpath) : strlen(real_old_fpath);

	if (strncmp(real_fpath, real_old_fpath, bigger) == 0) {
		m38_log_msg(LOG_WARN, "Cowardly refusing to {un,sym}link the same file to itself.");
		goto update_time;
	}

	if (get_file_creation_date(real_fpath) == 0) {
		m38_log_msg(LOG_WARN, "Cowardly refusing to symlink file to broken file.");
		goto update_time;
	}

	m38_log_msg(LOG_WARN, "Unlinking and creating a symlink from '%s' to '%s'.",
			real_fpath, real_old_fpath);

	/* Unlink new file. */
	if (unlink(real_fpath) == -1)
		m38_log_msg(LOG_ERR, "Could not delete '%s'.", real_fpath);

	/* Symlink to old file. */
	if (symlink(real_old_fpath, real_fpath) == -1)
		m38_log_msg(LOG_ERR, "Could not create symlink from '%s' to '%s'.",
				real_fpath, real_old_fpath);

update_time: ; /* Yes the semicolon is necessary. Fucking C. */
	/* Update timestamp on symlink to reflect what the real timestamp is (the one
	 * on the alias). */
	struct timeval _new_time = {
		.tv_sec = new_stamp,
		.tv_usec = 0
	};

	/* POSIX is fucking weird, man: */
	const struct timeval _new_times[] = { _new_time, _new_time };
	if (lutimes(real_fpath, _new_times) != 0) {
		m38_log_msg(LOG_WARN, "Unable to set timesteamp on new symlink.");
		perror("Alias symlink timestamp update");
	}

	free(real_fpath);
	free(real_old_fpath);
}
int add_image_to_db(const char *file_path, const char *filename, const char board[MAX_BOARD_NAME_SIZE],
		const char post_key[MAX_KEY_SIZE], char out_webm_key[static MAX_KEY_SIZE]) {
	char image_hash[HASH_IMAGE_STR_SIZE] = {0};
	if (!hash_file(file_path, image_hash)) {
		m38_log_msg(LOG_ERR, "Could not hash '%s'.", file_path);
		return 0;
	}

	webm *_old_webm = get_image_by_oleg_key(image_hash, out_webm_key);

	if (!_old_webm) {
		int rc = _insert_webm(file_path, filename, image_hash, board, post_key);
		if (!rc)
			m38_log_msg(LOG_ERR, "Something went wrong inserting webm.");
		return rc;
	} else {
		/* This is the wrong key, we're going to use a different one. */
		memset(out_webm_key, '\0', MAX_KEY_SIZE);
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
	webm_alias *_old_alias = get_aliased_image(file_path, out_webm_key);
	int rc = 1;
	/* If we DONT already have an alias for this image with this filename,
	 * insert one.
	 * We know if this filename is an alias already because the hash of
	 * an alias is it's filename.
	 */
	if (_old_alias == NULL) {
		rc = _insert_aliased_webm(file_path, filename, image_hash, board, post_key);
		m38_log_msg(LOG_FUN, "%s (%s) is a new alias of %s (%s).", filename, board, _old_webm->filename, _old_webm->board);
	} else {
		m38_log_msg(LOG_WARN, "%s is already marked as an alias of %s. Old alias is: '%s'",
				file_path, _old_webm->filename, _old_alias->filename);
	}

	/* Create or update the many2many between these two: */
	/* We should already have the alias key stored from up above. */
	associate_alias_with_webm(_old_webm, out_webm_key);

	if (rc) {
		/* We don't want old alias, we want current alias here. Otherwise all
		 * of the timestamps are going to be the same as old_alias's.
		 */
		if (_old_alias == NULL) {
			time_t new_stamp = get_file_creation_date(file_path);
			if (new_stamp == 0) {
				m38_log_msg(LOG_ERR, "Could not stat new alias.");
			}

			modify_aliased_file(file_path, _old_webm, new_stamp);
		} else {
			modify_aliased_file(file_path, _old_webm, _old_alias->created_at);
		}
	} else
		m38_log_msg(LOG_ERR, "Something went wrong when adding image to db.");
	free(_old_alias);
	free(_old_webm);
	return rc;
}

struct thread *get_thread(const char key[static MAX_KEY_SIZE]) {
	size_t json_size = 0;
	char *json = (char *)fetch_data_from_db(&oleg_conn, key, &json_size);

	if (json == NULL)
		return NULL;

	thread *_thread = deserialize_thread(json);
	free(json);
	return _thread;
}

struct post *get_post(const char key[static MAX_KEY_SIZE]) {
	size_t json_size = 0;
	char *json = (char *)fetch_data_from_db(&oleg_conn, key, &json_size);

	if (json == NULL)
		return NULL;

	post *_post = deserialize_post(json);
	free(json);
	return _post;
}

static int _insert_thread(const char key[static MAX_KEY_SIZE], const thread *to_save) {
	char *serialized = serialize_thread(to_save);
	m38_log_msg(LOG_INFO, "Serialized thread: %s", serialized);

	int ret = store_data_in_db(&oleg_conn, key, (unsigned char *)serialized, strlen(serialized));
	free(serialized);

	return ret;
}

static int _insert_post(const char key[static MAX_KEY_SIZE], const post *to_save) {
	char *serialized = serialize_post(to_save);
	m38_log_msg(LOG_INFO, "Serialized post: %s", serialized);

	int ret = store_data_in_db(&oleg_conn, key, (unsigned char *)serialized, strlen(serialized));
	if (ret != 1) {
		m38_log_msg(LOG_ERR, "Could not store post in database. Ret code: %i", ret);
	}
	free(serialized);

	return ret;
}

int add_post_to_db(const struct post_match *p_match, const char webm_key[static MAX_KEY_SIZE]) {
	if (!p_match)
		return 1;

	char post_key[MAX_KEY_SIZE] = {0};
	create_post_key(p_match->board, p_match->post_date, post_key);

	m38_log_msg(LOG_INFO, "Creating post with key: %s", post_key);

	post *existing_post = get_post(post_key);
	if (existing_post != NULL) {
		/* We already have this post saved. */
		vector_free(existing_post->replied_to_keys);
		free(existing_post->body_content);
		free(existing_post);

		return 0;
	}

	/* 1. Create thread key */
	char thread_key[MAX_KEY_SIZE] = {0};
	create_thread_key(p_match->board, p_match->thread_number, thread_key);

	/* 2. Check database for existing thread */
	thread *existing_thread = get_thread(thread_key);
	if (existing_thread == NULL) {
		/* 3. Create it if it doesn't exist */
		thread _new_thread = {
			.board = {0},
			._null_term_hax_1 = 0,
			.post_keys = vector_new(MAX_KEY_SIZE, 2)
		};

		existing_thread = malloc(sizeof(struct thread));
		strncpy(_new_thread.board, p_match->board, sizeof(_new_thread.board));
		memcpy(existing_thread, &_new_thread, sizeof(struct thread));
	}

	/* 4. There is no 4. */
	/* 5. Add new post key to thread foreign keys */
	unsigned int i;
	int found = 0;
	for (i = 0; i < existing_thread->post_keys->count; i++) {
		const char *existing = vector_get(existing_thread->post_keys, i);
		if (strncmp(existing, post_key, MAX_KEY_SIZE) == 0) {
			found = 1;
			break;
		}
	}
	if (!found) {
		vector_append(existing_thread->post_keys, post_key, sizeof(post_key));
		/* 6. Save thread object */
		_insert_thread(thread_key, existing_thread);
	}
	vector_free(existing_thread->post_keys);
	free(existing_thread);

	/* 7. Create post, Add thread key to post */
	post to_insert = {
		.post_id = {0},
		._null_term_hax_1 = 0,
		.thread_key = {0},
		._null_term_hax_2 = 0,
		.board = {0},
		._null_term_hax_3 = 0,
		.webm_key = {0},
		._null_term_hax_4 = 0,
		.post_no = {0},
		._null_term_hax_5 = 0,
		.body_content = NULL,
		.replied_to_keys = vector_new(MAX_KEY_SIZE, 2)
	};

	strncpy(to_insert.post_id, p_match->post_date, sizeof(to_insert.post_id));
	strncpy(to_insert.post_no, p_match->post_no, sizeof(to_insert.post_no));
	strncpy(to_insert.thread_key, thread_key, sizeof(to_insert.thread_key));
	strncpy(to_insert.board, p_match->board, sizeof(to_insert.board));
	strncpy(to_insert.webm_key, webm_key, sizeof(to_insert.webm_key));

	if (p_match->body_content != NULL)
		to_insert.body_content = strdup(p_match->body_content);

	/* 8. Save post object */
	_insert_post(post_key, &to_insert);
	free(to_insert.body_content);
	vector_free(to_insert.replied_to_keys);
	return 0;
}

int associate_alias_with_webm(const webm *webm, const char alias_key[static MAX_KEY_SIZE]) {
	if (!webm || strlen(alias_key) == 0)
		return 0;

	char key[MAX_KEY_SIZE] = {0};
	create_webm_to_alias_key(webm->file_hash, key);

	size_t json_size = 0;
	char *w2a_json = (char *)fetch_data_from_db(&oleg_conn, key, &json_size);
	if (!w2a_json) {
		/* No existing m2m relation. */
		vector *aliases = vector_new(MAX_KEY_SIZE, 1);
		vector_append(aliases, alias_key, strlen(alias_key));

		webm_to_alias _new_m2m = {
			.aliases = aliases
		};

		const char *serialized = serialize_webm_to_alias(&_new_m2m);
		if (!serialized) {
			m38_log_msg(LOG_ERR, "Could not serialize new webm_to_alias.");
			vector_free(aliases);
			return 0;
		}

		if (!store_data_in_db(&oleg_conn, key, (unsigned char *)serialized, strlen(serialized))) {
			m38_log_msg(LOG_ERR, "Could not store new webm_to_alias.");
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
		m38_log_msg(LOG_ERR, "Could not deserialize webm_to_alias.");
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
		m38_log_msg(LOG_ERR, "Could not serialize webm_to_alias.");
		return 0;
	}

	if (!store_data_in_db(&oleg_conn, key, (unsigned char *)new_serialized, strlen(new_serialized))) {
		m38_log_msg(LOG_ERR, "Could not store updated webm_to_alias.");
		free(new_serialized);
		return 0;
	}

	free(new_serialized);
	return 1;
}
