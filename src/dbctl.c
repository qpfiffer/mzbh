// vim: noet ts=4 sw=4
#ifdef __clang__
	#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#endif
#include <assert.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

#include <38-moths/logging.h>
#include <oleg-http/oleg-http.h>

#include "common_defs.h"
#include "db.h"
#include "models.h"
#include "sha3api_ref.h"
#include "utils.h"

static const char *WEBMS_LOCATION = NULL;

static int _add_directory(const char *directory_to_open, const char board[MAX_BOARD_NAME_SIZE]) {
	DIR *dirstream = opendir(directory_to_open);
	int total = 0;
	while (1) {
		struct dirent *result = readdir(dirstream);
		if (!result)
			break;
		if (result->d_name[0] != '.' && endswith(result->d_name, ".webm")) {
			int attempts = 0;
			int max_attempts = 3;
			for (;attempts < max_attempts; attempts++) {
				const size_t full_path_siz = strlen(directory_to_open) + strlen("/") + strlen(result->d_name) + 1;
				char full_path[full_path_siz];
				memset(full_path, '\0', full_path_siz);
				sprintf(full_path, "%s/%s", directory_to_open, result->d_name);

				char webm_key[MAX_KEY_SIZE] = {0};
				int rc = add_image_to_db(full_path, result->d_name, board, "-1", webm_key);
				if (rc)
					break;
				else {
					m38_log_msg(LOG_WARN, "(%i/%i) Could not add image to db. Retrying...",
						attempts+1, max_attempts);
				}
				total++;
			}
		}
	}
	closedir(dirstream);

	return total;
}

static int full_scan() {
	DIR *dirstream = opendir(webm_location());
	int total = 0;
	while (1) {
		struct dirent *result = readdir(dirstream);
		if (!result)
			break;
		/* We have to use stat here because d_type isn't supported on all filesystems. */
		char dir_name[255] = {0};

		snprintf(dir_name, sizeof(dir_name), "%s/%s", webm_location(), result->d_name);
		struct stat st = {0};
		stat(dir_name, &st);

		/* webms are organized by board */
		if (result->d_name[0] != '.' && S_ISDIR(st.st_mode)) {
			/* +1 for \0, +1 for an extra / */
			const size_t FULLPATH_SIZ = strlen(WEBMS_LOCATION) + strlen(result->d_name) + 2;
			char full_path[FULLPATH_SIZ];
			char board_name[MAX_BOARD_NAME_SIZE] = {0};
			memset(full_path, '\0', FULLPATH_SIZ);
			snprintf(full_path, FULLPATH_SIZ, "%s/%s", WEBMS_LOCATION, result->d_name);
			strncpy(board_name, result->d_name, MAX_BOARD_NAME_SIZE);

			total += _add_directory(full_path, board_name);
		}
	}
	closedir(dirstream);

	return total;
}

static int _webm_count() {
	m38_log_msg(LOG_INFO, "Num-matches: %i", webm_count());
	return 1;
}

static int _alias_count() {
	m38_log_msg(LOG_INFO, "Num-matches: %i", webm_alias_count());
	return 1;
}

static int _print_alias_matches() {
	char p[MAX_KEY_SIZE] = "alias";
	db_key_match *matches = fetch_matches_from_db(&oleg_conn, p);

	db_key_match *current = matches;
	while (current) {
		db_key_match *next = current->next;
		m38_log_msg(LOG_FUN, "%s", current->key);
		free(current);
		current = next;
	}

	return 1;
}

static inline int _f_ds_webms(const unsigned char *data, const size_t dsize, const void *e, void **extradata) {
	UNUSED(dsize);
	UNUSED(e);
	UNUSED(*extradata);
	webm *_webm = deserialize_webm((char *)data);
	m38_log_msg(LOG_FUN, "%s", _webm->filename);
	free(_webm);
	return 1;
}

static inline int _dead_webms(const unsigned char *data, const size_t dsize, const void *e, void **extradata) {
	UNUSED(dsize);
	UNUSED(e);
	UNUSED(*extradata);
	webm *_webm = deserialize_webm((char *)data);

	char *file_path = get_full_path_for_webm(_webm->board, _webm->filename);
	struct stat st = {0};
	if (stat(file_path, &st) == -1)
		m38_log_msg(LOG_FUN, "'%s' does not exist.", file_path);
	free(file_path);
	free(_webm);
	return 1;
}

static int _print_webm_filenames() {
	char p[MAX_KEY_SIZE] = WEBM_NMSPC;
	db_match *matches = filter(&oleg_conn, p, NULL, &_f_ds_webms);

	db_match *cur = matches;
	while (cur) {
		db_match *to_free = cur;
		cur = cur->next;
		free((unsigned char *)to_free->data);
		free(to_free);
	}
	return 1;
}

static int _dead_files() {
	char p[MAX_KEY_SIZE] = WEBM_NMSPC;
	db_match *matches = filter(&oleg_conn, p, NULL, &_dead_webms);

	db_match *cur = matches;
	while (cur) {
		db_match *to_free = cur;
		cur = cur->next;
		free((unsigned char *)to_free->data);
		free(to_free);
	}
	return 1;
}

typedef struct cmd {
	const char *cmd;
	int (*func_ptr)();
	const char *help;
} cmd;

const cmd commands[] = {
	{"full_scan", &full_scan, "Scans the entire webm directory and makes sure everything is in the DB."},
	{"webm_count", &_webm_count, "Gets the number of webms in the database."},
	{"alias_count", &_alias_count, "Gets the number of aliases in the database."},
	{"print_alias_matches", &_print_alias_matches, "Prints all alias keys in the database."},
	{"print_webm_filenames", &_print_webm_filenames, "Prints filenames of all the webms."},
	{"dead_webms", &_dead_files, "Prints webms that are in the db but not on the FS."}
};

static void usage(const char *program_name) {
	m38_log_msg(LOG_ERR, "Usage: %s <command>", program_name);
	unsigned int i;
	for (i = 0; i < sizeof(commands)/sizeof(commands[0]); i++) {
		m38_log_msg(LOG_ERR, "%s	--	%s", commands[i].cmd, commands[i].help);
	}
}


int main(int argc, char *argv[]) {
	if (argc == 1) {
		usage(argv[0]);
		return 1;
	}

	WEBMS_LOCATION = webm_location();

	m38_log_msg(LOG_INFO, "Using webms dir: %s", WEBMS_LOCATION);

	int i = 1;
	for (;i < argc; i++) {
		const char *current_arg = argv[i];

		unsigned int j;
		for (j = 0; j < (sizeof(commands)/sizeof(commands[0])); j++) {
			const cmd current_command = commands[j];
			if (strncmp(current_arg, current_command.cmd, strlen(current_command.cmd)) == 0) {
				current_command.func_ptr();
				return 0;
			}
		}
	}

	usage(argv[0]);
	return 1;
}
