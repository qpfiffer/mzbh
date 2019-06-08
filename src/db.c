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
#include "parson.h"
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

	res = PQexec(conn, query_command);

	if (PQresultStatus(res) != PGRES_TUPLES_OK) {
		m38_log_msg(LOG_ERR, "SELECT failed: %s", PQerrorMessage(conn));
		goto error;
	}

	char *bytes = PQgetvalue(res, 0, 0);
	ret = atol(bytes);

	_finish_pg_connection(conn);

	return ret;

error:
	if (res)
		PQclear(res);
	_finish_pg_connection(conn);
	return 0;
}

PGresult *get_posts_by_thread_id(const unsigned int id) {
	PGresult *res = NULL;
	PGconn *conn = NULL;

	char id_buf[64] = {0};
	snprintf(id_buf, sizeof(id_buf), "%d", id);

	const char *param_values[] = {id_buf};

	conn = _get_pg_connection();
	if (!conn)
		goto error;

	res = PQexecParams(conn,
					  "SELECT EXTRACT(EPOCH FROM p.created_at) AS created_at, p.*, w.filename AS w_filename, wa.filename AS wa_filename FROM posts AS p "
						"JOIN threads AS t ON p.thread_id = t.id "
						"FULL OUTER JOIN webms AS w ON w.post_id = p.id "
						"FULL OUTER JOIN webm_aliases AS wa ON wa.post_id = p.id "
						"WHERE t.id = $1"
						"ORDER BY fourchan_post_id DESC",
					  1,
					  NULL,
					  param_values,
					  NULL,
					  NULL,
					  0);

	if (PQresultStatus(res) != PGRES_TUPLES_OK) {
		m38_log_msg(LOG_ERR, "SELECT failed: %s", PQerrorMessage(conn));
		goto error;
	}

	_finish_pg_connection(conn);

	return res;

error:
	if (res)
		PQclear(res);
	_finish_pg_connection(conn);
	return NULL;
}

PGresult *get_aliases_by_webm_id(const unsigned int id) {
	PGresult *res = NULL;
	PGconn *conn = NULL;

	char id_buf[64] = {0};
	snprintf(id_buf, sizeof(id_buf), "%d", id);

	const char *param_values[] = {id_buf};

	conn = _get_pg_connection();
	if (!conn)
		goto error;

	res = PQexecParams(conn,
					  "SELECT EXTRACT(EPOCH FROM a.created_at) AS created_at, a.* FROM webm_aliases AS a "
					  "WHERE a.webm_id = $1 "
						"ORDER BY EXTRACT(EPOCH FROM a.created_at) DESC",
					  1,
					  NULL,
					  param_values,
					  NULL,
					  NULL,
					  0);

	if (PQresultStatus(res) != PGRES_TUPLES_OK) {
		m38_log_msg(LOG_ERR, "SELECT failed: %s", PQerrorMessage(conn));
		goto error;
	}

	_finish_pg_connection(conn);

	return res;

error:
	if (res)
		PQclear(res);
	_finish_pg_connection(conn);
	return NULL;
}

PGresult *get_images_by_popularity(const unsigned int offset, const unsigned int limit) {
	PGresult *res = NULL;
	PGconn *conn = NULL;

	conn = _get_pg_connection();
	if (!conn)
		goto error;

	char lim_buf[64] = {0};
	snprintf(lim_buf, sizeof(lim_buf), "%d", limit);

	char off_buf[64] = {0};
	snprintf(off_buf, sizeof(off_buf), "%d", offset);

	const char *param_values[] = {lim_buf, off_buf};

	res = PQexecParams(conn,
					"SELECT EXTRACT(EPOCH FROM webms.created_at) AS created_at, webms.* "
					"FROM webms LEFT JOIN webm_aliases ON webms.id = webm_aliases.webm_id "
					"GROUP BY webms.id "
					"ORDER BY count(webm_aliases.id) DESC "
					"LIMIT $1 OFFSET $2;",
					2,
					NULL,
					param_values,
					NULL,
					NULL,
					0);

	if (PQresultStatus(res) != PGRES_TUPLES_OK) {
		m38_log_msg(LOG_ERR, "SELECT failed: %s", PQerrorMessage(conn));
		goto error;
	}

	_finish_pg_connection(conn);

	return res;

error:
	if (res)
		PQclear(res);
	_finish_pg_connection(conn);
	return NULL;
}
webm *get_image_by_oleg_key(const char image_hash[static HASH_ARRAY_SIZE], char out_key[static MAX_KEY_SIZE]) {
	PGresult *res = NULL;
	PGconn *conn = NULL;

	create_webm_key(image_hash, out_key);
	const char *param_values[] = {image_hash};

	conn = _get_pg_connection();
	if (!conn)
		goto error;

	res = PQexecParams(conn,
					  "SELECT EXTRACT(EPOCH FROM created_at) AS created_at, * FROM webms WHERE file_hash = $1",
					  1,
					  NULL,
					  param_values,
					  NULL,
					  NULL,
					  0);

	if (PQresultStatus(res) != PGRES_TUPLES_OK) {
		m38_log_msg(LOG_ERR, "SELECT failed: %s", PQerrorMessage(conn));
		goto error;
	}

	webm *deserialized = deserialize_webm_from_tuples(res, 0);
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

unsigned int set_image(const webm *webm) {
	char key[MAX_KEY_SIZE] = {0};
	create_webm_key(webm->file_hash, key);

	PGresult *res = NULL;
	PGconn *conn = NULL;

	conn = _get_pg_connection();
	if (!conn)
		goto error;

	char post_id_buf[64] = {0};
	snprintf(post_id_buf, sizeof(post_id_buf), "%lu", webm->post_id);

	char size_buf[64] = {0};
	snprintf(size_buf, sizeof(size_buf), "%ld", webm->size);

	const char *param_values[] = {
		key,
		webm->file_hash,
		webm->filename,
		webm->board,
		webm->file_path,
		post_id_buf,
		size_buf
	};
	res = PQexecParams(conn,
					  "INSERT INTO webms (oleg_key, file_hash, filename,"
					  "board, file_path, post_id, size)"
					  "VALUES ($1, $2, $3, $4, $5, $6, $7) "
					  "RETURNING id;",
					  7,
					  NULL,
					  param_values,
					  NULL,
					  NULL,
					  0);

	if (PQresultStatus(res) != PGRES_TUPLES_OK) {
		m38_log_msg(LOG_ERR, "SELECT failed: %s", PQerrorMessage(conn));
		goto error;
	}

	if (PQntuples(res) <= 0)
		goto error;

	unsigned int id = atol(PQgetvalue(res, 0, 0));

	PQclear(res);
	_finish_pg_connection(conn);

	return id;

error:
	if (res)
		PQclear(res);
	_finish_pg_connection(conn);
	return 0;
}

webm_alias *get_aliased_image_with_key(const char key[static MAX_KEY_SIZE]) {
	PGresult *res = NULL;
	PGconn *conn = NULL;

	const char *param_values[] = {key};

	conn = _get_pg_connection();
	if (!conn)
		goto error;

	res = PQexecParams(conn,
					  "SELECT * FROM webm_aliases WHERE oleg_key = $1",
					  1,
					  NULL,
					  param_values,
					  NULL,
					  NULL,
					  0);

	if (PQresultStatus(res) != PGRES_TUPLES_OK) {
		m38_log_msg(LOG_ERR, "SELECT failed: %s", PQerrorMessage(conn));
		goto error;
	}

	webm_alias *deserialized = deserialize_alias_from_tuples(res, 0);
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

webm_alias *get_aliased_image_by_oleg_key(const char filepath[static MAX_IMAGE_FILENAME_SIZE], char out_key[static MAX_KEY_SIZE]) {
	create_alias_key(filepath, out_key);

	return get_aliased_image_with_key(out_key);
}


// webm_to_alias *get_webm_to_alias(const char image_hash[static HASH_ARRAY_SIZE]) {
// 	char key[MAX_KEY_SIZE] = {0};
// 	create_webm_to_alias_key(image_hash, key);
//
// 	size_t json_size = 0;
// 	char *json = (char *)fetch_data_from_db(&oleg_conn, key, &json_size);
//
// 	if (json == NULL)
// 		return NULL;
//
// 	webm_to_alias *w2a = deserialize_webm_to_alias(json);
// 	free(json);
// 	return w2a;
// }

unsigned int set_aliased_image(const webm_alias *webm) {
	char key[MAX_KEY_SIZE] = {0};
	create_alias_key(webm->file_hash, key);

	PGresult *res = NULL;
	PGconn *conn = NULL;

	conn = _get_pg_connection();
	if (!conn)
		goto error;

	char post_id_buf[64] = {0};
	snprintf(post_id_buf, sizeof(post_id_buf), "%lu", webm->post_id);

	char webm_id_buf[64] = {0};
	snprintf(webm_id_buf, sizeof(webm_id_buf), "%lu", webm->webm_id);

	const char *param_values[] = {
		key,
		webm->file_hash,
		webm->filename,
		webm->board,
		webm->file_path,
		post_id_buf,
		webm_id_buf
	};
	res = PQexecParams(conn,
					  "INSERT INTO webm_aliases (oleg_key, file_hash, filename,"
					  "board, file_path, post_id, webm_id)"
					  "VALUES ($1, $2, $3, $4, $5, $6, $7) "
					  "RETURNING id;",
					  7,
					  NULL,
					  param_values,
					  NULL,
					  NULL,
					  0);

	if (PQresultStatus(res) != PGRES_TUPLES_OK) {
		m38_log_msg(LOG_ERR, "SELECT failed: %s", PQerrorMessage(conn));
		goto error;
	}

	if (PQntuples(res) <= 0)
		goto error;

	unsigned int id = atol(PQgetvalue(res, 0, 0));

	PQclear(res);
	_finish_pg_connection(conn);

	return id;

error:
	if (res)
		PQclear(res);
	_finish_pg_connection(conn);
	return 0;
}


static int _insert_webm(const char *file_path, const char filename[static MAX_IMAGE_FILENAME_SIZE],
						const char image_hash[static HASH_IMAGE_STR_SIZE], const char board[static MAX_BOARD_NAME_SIZE],
						const unsigned int post_id) {
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
		.post_id = post_id
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
								const char board[static MAX_BOARD_NAME_SIZE],
								const unsigned int post_id,
								const unsigned int webm_id) {
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
		.post_id = post_id,
		.webm_id = webm_id
	};

	memcpy(to_insert.file_hash, image_hash, sizeof(to_insert.file_hash));
	memcpy(to_insert.filename, filename, sizeof(to_insert.filename));
	memcpy(to_insert.file_path, file_path, sizeof(to_insert.file_path));
	memcpy(to_insert.board, board, sizeof(to_insert.board));

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
		const unsigned int post_id) {
	char image_hash[HASH_IMAGE_STR_SIZE] = {0};
	if (!hash_file(file_path, image_hash)) {
		m38_log_msg(LOG_ERR, "Could not hash '%s'.", file_path);
		return 0;
	}

	char out_webm_key[MAX_KEY_SIZE] = {0};
	webm *_old_webm = get_image_by_oleg_key(image_hash, out_webm_key);

	if (!_old_webm) {
		int rc = _insert_webm(file_path, filename, image_hash, board, post_id);
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
	webm_alias *_old_alias = get_aliased_image_by_oleg_key(file_path, out_webm_key);
	int rc = 1;
	/* If we DONT already have an alias for this image with this filename,
	 * insert one.
	 * We know if this filename is an alias already because the hash of
	 * an alias is it's filename.
	 */
	if (_old_alias == NULL) {
		rc = _insert_aliased_webm(file_path, filename, image_hash, board, post_id, _old_webm->id);
		m38_log_msg(LOG_FUN, "%s (%s) is a new alias of %s (%s).", filename, board, _old_webm->filename, _old_webm->board);
	} else {
		m38_log_msg(LOG_WARN, "%s is already marked as an alias of %s. Old alias is: '%s'",
				file_path, _old_webm->filename, _old_alias->filename);
	}

	/* Create or update the many2many between these two: */
	/* We should already have the alias key stored from up above. */
	//associate_alias_with_webm(_old_webm, out_webm_key);

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

	free(_old_alias);
	free(_old_webm);
	return rc;
}

struct thread *get_thread_by_id(const unsigned int thread_id) {
	PGresult *res = NULL;
	PGconn *conn = NULL;

	conn = _get_pg_connection();
	if (!conn)
		goto error;

	char id_buf[64] = {0};
	snprintf(id_buf, sizeof(id_buf), "%d", thread_id);
	const char *param_values[] = {id_buf};

	res = PQexecParams(conn,
					  "SELECT * FROM threads WHERE id = $1",
					  1,
					  NULL,
					  param_values,
					  NULL,
					  NULL,
					  0);

	if (PQresultStatus(res) != PGRES_TUPLES_OK) {
		m38_log_msg(LOG_ERR, "SELECT failed: %s", PQerrorMessage(conn));
		goto error;
	}

	thread *deserialized = deserialize_thread_from_tuples(res, 0);
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


struct post *get_post(const unsigned int post_id) {
	PGresult *res = NULL;
	PGconn *conn = NULL;

	conn = _get_pg_connection();
	if (!conn)
		goto error;

	char post_id_buf[64] = {0};
	snprintf(post_id_buf, sizeof(post_id_buf), "%d", post_id);
	const char *param_values[] = {post_id_buf};
	res = PQexecParams(conn,
					  "SELECT * FROM posts WHERE id = $1",
					  1,
					  NULL,
					  param_values,
					  NULL,
					  NULL,
					  0);

	if (PQresultStatus(res) != PGRES_TUPLES_OK) {
		m38_log_msg(LOG_ERR, "SELECT failed: %s", PQerrorMessage(conn));
		goto error;
	}

	post *deserialized = deserialize_post_from_tuples(res, 0);
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

static unsigned int get_post_id_by_oleg_key(const char post_key[static MAX_KEY_SIZE]) {
	PGresult *res = NULL;
	PGconn *conn = NULL;

	conn = _get_pg_connection();
	if (!conn)
		goto error;

	const char *param_values[] = {post_key};
	res = PQexecParams(conn,
					  "SELECT id FROM posts WHERE oleg_key = $1",
					  1,
					  NULL,
					  param_values,
					  NULL,
					  NULL,
					  0);

	if (PQresultStatus(res) != PGRES_TUPLES_OK) {
		m38_log_msg(LOG_ERR, "SELECT failed: %s", PQerrorMessage(conn));
		goto error;
	}

	if (PQntuples(res) <= 0)
		goto error;

	unsigned int val = atol(PQgetvalue(res, 0, 0));

	PQclear(res);
	_finish_pg_connection(conn);

	return val;

error:
	if (res)
		PQclear(res);
	_finish_pg_connection(conn);
	return 0;
}

static unsigned int get_thread_id_for_oleg_key(const char key[static MAX_KEY_SIZE]) {
	PGresult *res = NULL;
	PGconn *conn = NULL;

	conn = _get_pg_connection();
	if (!conn)
		goto error;

	const char *param_values[] = {key};
	res = PQexecParams(conn,
					  "SELECT id FROM threads WHERE oleg_key = $1",
					  1,
					  NULL,
					  param_values,
					  NULL,
					  NULL,
					  0);

	if (PQresultStatus(res) != PGRES_TUPLES_OK) {
		m38_log_msg(LOG_ERR, "SELECT failed: %s", PQerrorMessage(conn));
		goto error;
	}

	if (PQntuples(res) <= 0)
		goto error;

	unsigned int id = atol(PQgetvalue(res, 0, 0));

	PQclear(res);
	_finish_pg_connection(conn);

	return id;

error:
	if (res)
		PQclear(res);
	_finish_pg_connection(conn);
	return 0;
}



static int _insert_thread(const thread *to_save) {
	PGresult *res = NULL;
	PGconn *conn = NULL;

	conn = _get_pg_connection();
	if (!conn)
		goto error;

	const char *param_values[] = {
		to_save->oleg_key,
		to_save->board,
		to_save->subject
	};
	res = PQexecParams(conn,
					  "INSERT INTO threads (oleg_key, board, subject)"
					  "VALUES ($1, $2, $3) "
					  "RETURNING id;",
					  3,
					  NULL,
					  param_values,
					  NULL,
					  NULL,
					  0);

	if (PQresultStatus(res) != PGRES_TUPLES_OK) {
		m38_log_msg(LOG_ERR, "SELECT failed: %s", PQerrorMessage(conn));
		goto error;
	}

	if (PQntuples(res) <= 0)
		goto error;

	unsigned int id = atol(PQgetvalue(res, 0, 0));

	PQclear(res);
	_finish_pg_connection(conn);

	return id;

error:
	if (res)
		PQclear(res);
	_finish_pg_connection(conn);
	return 0;
}

static int _insert_post(const post *to_save) {
	PGresult *res = NULL;
	PGconn *conn = NULL;

	conn = _get_pg_connection();
	if (!conn)
		goto error;

	char thread_id[256] = {0};
	snprintf(thread_id, sizeof(thread_id), "%lu", to_save->thread_id);

	char fourchan_post_id[256] = {0};
	snprintf(fourchan_post_id, sizeof(fourchan_post_id), "%lu", to_save->fourchan_post_id);

	char fourchan_post_no[256] = {0};
	snprintf(fourchan_post_no, sizeof(fourchan_post_no), "%lu", to_save->fourchan_post_no);

	JSON_Value *thread_keys = json_value_init_array();
	JSON_Array *thread_keys_array = json_value_get_array(thread_keys);

	unsigned int i;
	for (i = 0; i < to_save->replied_to_keys->count; i++)
		json_array_append_string(thread_keys_array,
				vector_get(to_save->replied_to_keys, i));

	char *replied_to_json = json_serialize_to_string(thread_keys);

	json_value_free(thread_keys);

	const char *param_values[] = {
		to_save->oleg_key,
		fourchan_post_id,
		fourchan_post_no,
		thread_id,
		to_save->board,
		to_save->body_content,
		replied_to_json
	};
	res = PQexecParams(conn,
					  "INSERT INTO posts "
					  "(oleg_key, fourchan_post_id, fourchan_post_no, thread_id, board,"
					  " body_content, replied_to_keys)"
					  "VALUES ($1, $2, $3, $4, $5, $6, $7) "
					  "RETURNING id;",
					  7,
					  NULL,
					  param_values,
					  NULL,
					  NULL,
					  0);

	free(replied_to_json);

	if (PQresultStatus(res) != PGRES_TUPLES_OK) {
		m38_log_msg(LOG_ERR, "SELECT failed: %s", PQerrorMessage(conn));
		goto error;
	}

	if (PQntuples(res) <= 0)
		goto error;

	unsigned int id = atol(PQgetvalue(res, 0, 0));

	PQclear(res);
	_finish_pg_connection(conn);

	return id;

error:
	if (res)
		PQclear(res);
	_finish_pg_connection(conn);
	return 0;

}

unsigned int add_post_to_db(const struct post_match *p_match) {
	if (!p_match)
		return 1;

	char post_key[MAX_KEY_SIZE] = {0};
	create_post_key(p_match->board, p_match->post_date, post_key);

	unsigned int existing_post_id = get_post_id_by_oleg_key(post_key);
	if (existing_post_id) {
		/* We already have this post saved. */
		m38_log_msg(LOG_WARN, "Post %s already exists.", post_key);
		return existing_post_id;
	} else {
		m38_log_msg(LOG_INFO, "Creating post with key: %s", post_key);
	}

	/* 1. Create thread key */
	char thread_key[MAX_KEY_SIZE] = {0};
	create_thread_key(p_match->board, p_match->thread_number, thread_key);

	/* 2. Check database for existing thread */
	unsigned int thread_id = get_thread_id_for_oleg_key(thread_key);
	if (!thread_id) {
		/* 3. Create it if it doesn't exist */
		thread _new_thread = {
			.id = 0,
			.board = {0},
			._null_term_hax_1 = 0,
			.oleg_key = {0},
			._null_term_hax_2 = 0,
			.subject = NULL,
			.created_at = 0
		};

		strncpy(_new_thread.board, p_match->board, sizeof(_new_thread.board));
		strncpy(_new_thread.oleg_key, thread_key, sizeof(_new_thread.oleg_key));
		_new_thread.subject = strdup(p_match->subject);

		thread_id = _insert_thread(&_new_thread);

		free(_new_thread.subject);
	}

	/* 4. There is no 4. */

	/* 7. Create post, Add thread key to post */
	post to_insert = {
		.board = {0},
		._null_term_hax_1 = 0,
		.oleg_key = {0},
		._null_term_hax_2 = 0,
		.fourchan_post_no = 0,
		.fourchan_post_id = 0,
		.body_content = NULL,
		.replied_to_keys = vector_new(MAX_KEY_SIZE, 2),
		.id = 0,
		.thread_id = thread_id,
		.created_at = 0
	};

	to_insert.fourchan_post_id = atol(p_match->post_date);
	to_insert.fourchan_post_no = atol(p_match->post_no);
	strncpy(to_insert.board, p_match->board, sizeof(to_insert.board));
	strncpy(to_insert.oleg_key, post_key, sizeof(to_insert.oleg_key));

	if (p_match->body_content != NULL)
		to_insert.body_content = strdup(p_match->body_content);

	/* 8. Save post object */
	const unsigned int post_id = _insert_post(&to_insert);

	free(to_insert.body_content);
	vector_free(to_insert.replied_to_keys);
	return post_id;
}
