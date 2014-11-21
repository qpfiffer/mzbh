// vim: noet ts=4 sw=4
#include "btree.h"
#include "logging.h"

static void usage(const char *program_name) {
	log_msg(LOG_FUN, "Usage: %s <command>", program_name);
}

int main(int argc, char *argv[]) {
	usage(argv[0]);
	return 0;
}
