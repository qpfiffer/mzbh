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

/* Webm get/set stuff */
webm *get_image(const char image_hash[static HASH_ARRAY_SIZE]) {
	char key[MAX_KEY_SIZE] = {0};
	create_webm_key(image_hash, key);

	size_t json_size = 0;
	char *json = (char *)fetch_data_from_db(&oleg_conn, key, &json_size);
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

webm_alias *get_aliased_image(const char filepath[static MAX_IMAGE_FILENAME_SIZE]) {
	char key[MAX_KEY_SIZE] = {0};
	create_alias_key(filepath, key);

	return get_aliased_image_with_key(key);
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
	/* log_msg(LOG_INFO, "Serialized: %s", serialized); */

	int ret = store_data_in_db(&oleg_conn, key, (unsigned char *)serialized, strlen(serialized));
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
		log_msg(LOG_WARN, "Unlinking and creating a symlink from '%s' to '%s'.", file_path, _old_webm->file_path);
		if (unlink(file_path) == -1)
			log_msg(LOG_ERR, "Could not delete '%s'.", file_path);
		if (symlink(_old_webm->file_path, file_path) == -1)
			log_msg(LOG_ERR, "Could not create symlink from '%s' to '%s'.", file_path, _old_webm->file_path);
	} else
		log_msg(LOG_ERR, "Something went wrong when adding image to db.");
	free(_old_alias);
	free(_old_webm);
	return rc;
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
			log_msg(LOG_ERR, "Could not serialize new webm_to_alias.");
			vector_free(aliases);
			return 0;
		}

		if (!store_data_in_db(&oleg_conn, key, (unsigned char *)serialized, strlen(serialized))) {
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

	if (!store_data_in_db(&oleg_conn, key, (unsigned char *)new_serialized, strlen(new_serialized))) {
		log_msg(LOG_ERR, "Could not store updated webm_to_alias.");
		free(new_serialized);
		return 0;
	}

	free(new_serialized);
	return 1;
}
