// vim: noet ts=4 sw=4
#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>

#include "db.h"
#include "logging.h"
#include "models.h"
#include "sha3api_ref.h"
#include "utils.h"

int hash_image(const char *file_path, char outbuf[HASH_IMAGE_STR_SIZE]) {
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

char *fetch_data_from_db(const char key[MAX_KEY_SIZE]) {
	char *data = NULL;
	return data;
}

webm *get_image(const char image_hash[HASH_ARRAY_SIZE]) {
	char key_name[MAX_KEY_SIZE] = {0};
	create_webm_key(image_hash, key_name);
	char *json = fetch_data_from_db(key_name);
	log_msg(LOG_INFO, "Json from DB: %s", json);
	/* TODO: Deserialize webm from json here. */
	return NULL;
}

int add_image_to_db(const char *file_path, const char board[MAX_BOARD_NAME_SIZE]) {
	char image_hash[HASH_IMAGE_STR_SIZE] = {0};
	if (!hash_image(file_path, image_hash))
		return 1;

	webm *_old_webm = get_image(image_hash);
	if (_old_webm == NULL) {
		time_t modified_time = get_file_creation_date(file_path);
		if (modified_time == 0) {
			log_msg(LOG_ERR, "File does not exist.");
			return 1;
		}
		size_t size = get_file_size(file_path);
		if (size == 0) {
			log_msg(LOG_ERR, "File does not exist.");
			return 1;
		}

		webm to_insert = {
			.is_alias = 0,
			.file_hash = {0},
			.filename = {0},
			.board = {0},
			.created_at = modified_time,
			.size = size
		};
		memcpy(to_insert.file_hash, image_hash, sizeof(to_insert.file_hash));
		memcpy(to_insert.filename, file_path, sizeof(to_insert.filename));
		memcpy(to_insert.board, board, sizeof(to_insert.board));

		char *serialized = serialize_webm(&to_insert);
		log_msg(LOG_INFO, "Serialized: %s", serialized);
		free(serialized);
	} else {
	}
	return 0;
}
