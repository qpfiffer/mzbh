// vim: noet ts=4 sw=4
#include "btree.h"
#include "logging.h"
#include "sha3api_ref.h"
#include "utils.h"

static void usage(const char *program_name) {
	log_msg(LOG_ERR, "Usage: %s <command>", program_name);
}

int main(int argc, char *argv[]) {
	if (argc == 1) {
		usage(argv[0]);
		return 1;
	}

	const char *WEBMS_LOCATION = webm_location();
	const char *DB_LOCATION = db_location();

	log_msg(LOG_INFO, "Using webms dir: %s", WEBMS_LOCATION);
	log_msg(LOG_INFO, "Using db file: %s", DB_LOCATION);

	int i = 1;
	for (;i < argc; i++) {
		log_msg(LOG_FUN, "Arg: %s", argv[i]);
	}
	return 0;
}
