// vim: noet ts=4 sw=4
#include <dirent.h>
#include <string.h>
#include <stdio.h>

#include "btree.h"
#include "logging.h"
#include "sha3api_ref.h"
#include "utils.h"

static const char *WEBMS_LOCATION = NULL;
static const char *DB_LOCATION = NULL;


static void usage(const char *program_name) {
	log_msg(LOG_ERR, "Usage: %s <command>", program_name);
	log_msg(LOG_ERR, "full_scan		--	Scans the entire webm directory and makes sure everything is in the DB.", program_name);
}

static int _add_directory(const char *directory_to_open) {
	struct dirent dirent_thing = {0};

	DIR *dirstream = opendir(directory_to_open);
	int total = 0;
	while (1) {
		struct dirent *result = NULL;
		readdir_r(dirstream, &dirent_thing, &result);
		if (!result)
			break;
		if (result->d_name[0] != '.' && endswith(result->d_name, ".webm")) {
			/* total += _add_directory(result->d_name); */
			log_msg(LOG_FUN, "Adding %s...", result->d_name);
			total++;
		}
	}
	closedir(dirstream);

	return total;
}

static int full_scan() {
	struct dirent dirent_thing = {0};

	DIR *dirstream = opendir(WEBMS_LOCATION);
	int total = 0;
	while (1) {
		struct dirent *result = NULL;
		readdir_r(dirstream, &dirent_thing, &result);
		if (!result)
			break;
		/* webms are organized by board */
		if (result->d_name[0] != '.' && result->d_type == DT_DIR) {
			/* +1 for \0, +1 for an extra / */
			const size_t FULLPATH_SIZ = strlen(WEBMS_LOCATION) + strlen(result->d_name) + 2;
			char full_path[FULLPATH_SIZ];
			memset(full_path, '\0', FULLPATH_SIZ);
			snprintf(full_path, FULLPATH_SIZ, "%s/%s", WEBMS_LOCATION, result->d_name);

			total += _add_directory(full_path);
		}
	}
	closedir(dirstream);

	return total;
}

int main(int argc, char *argv[]) {
	if (argc == 1) {
		usage(argv[0]);
		return 1;
	}

	WEBMS_LOCATION = webm_location();
	DB_LOCATION = db_location();

	log_msg(LOG_INFO, "Using webms dir: %s", WEBMS_LOCATION);
	log_msg(LOG_INFO, "Using db file: %s", DB_LOCATION);

	int i = 1;
	for (;i < argc; i++) {
		const char *current_arg = argv[i];

		if (strncmp(current_arg, "full_scan", strlen("full_scan")) == 0) {
			full_scan();
		}
	}
	return 0;
}
