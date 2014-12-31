// vim: noet ts=4 sw=4
#include <assert.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

#include "common_defs.h"
#include "db.h"
#include "logging.h"
#include "sha3api_ref.h"
#include "utils.h"

static const char *WEBMS_LOCATION = NULL;


static void usage(const char *program_name) {
	log_msg(LOG_ERR, "Usage: %s <command>", program_name);
	log_msg(LOG_ERR, "full_scan		--	Scans the entire webm directory and makes sure everything is in the DB.");
	log_msg(LOG_ERR, "alias_count	--	Gets the number of aliases in the database.");
	log_msg(LOG_ERR, "webm_count	--	Gets the number of webms in the database.");
}

static int _add_directory(const char *directory_to_open, const char board[MAX_BOARD_NAME_SIZE]) {
	struct dirent dirent_thing = {0};

	DIR *dirstream = opendir(directory_to_open);
	int total = 0;
	while (1) {
		struct dirent *result = NULL;
		readdir_r(dirstream, &dirent_thing, &result);
		if (!result)
			break;
		if (result->d_name[0] != '.' && endswith(result->d_name, ".webm")) {
			const size_t full_path_siz = strlen(directory_to_open) + strlen(result->d_name);
			char full_path[full_path_siz];
			memset(full_path, '\0', full_path_siz);
			sprintf(full_path, "%s/%s", directory_to_open, result->d_name);

			assert(add_image_to_db(full_path, result->d_name, board));
			total++;
		}
	}
	closedir(dirstream);

	return total;
}

static int full_scan() {
	struct dirent dirent_thing = {0};

	DIR *dirstream = opendir(webm_location());
	int total = 0;
	while (1) {
		struct dirent *result = NULL;
		readdir_r(dirstream, &dirent_thing, &result);
		if (!result)
			break;
		/* We have to use stat here because d_type isn't supported on all filesystems. */
		char dir_name[64] = {0};

		sprintf(dir_name, "%s/%s", webm_location(), result->d_name);
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

static int x_count(const char prefix[static MAX_KEY_SIZE]) {
	size_t dsize = 0;

	unsigned char *data = fetch_matches_from_db(prefix, &dsize);
	if (!data)
		return 0;

	unsigned int i, matches = 0;
	for (i = 0; i < dsize; i++) {
		if (data[i] == '\n')
			matches++;
	}

	log_msg(LOG_INFO, "Num-matches: %i\n", matches);
	/* printf("All matches: \n%s\n", data); */
	free(data);
	return 1;
}

static int webm_count() {
	char prefix[MAX_KEY_SIZE] = WEBM_NMSPC;
	return x_count(prefix);
}

static int alias_count() {
	char prefix[MAX_KEY_SIZE] = ALIAS_NMSPC;
	return x_count(prefix);
}

typedef struct cmd {
	const char *cmd;
	int (*func_ptr)();
} cmd;

/* TODO: Add help text to these. */
const cmd commands[] = {
	{"full_scan", &full_scan},
	{"webm_count", &webm_count},
	{"alias_count", &alias_count},
};

int main(int argc, char *argv[]) {
	if (argc == 1) {
		usage(argv[0]);
		return 1;
	}

	WEBMS_LOCATION = webm_location();

	log_msg(LOG_INFO, "Using webms dir: %s", WEBMS_LOCATION);

	int i = 1;
	for (;i < argc; i++) {
		const char *current_arg = argv[i];

		int j;
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
