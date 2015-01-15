// vim: noet ts=4 sw=4

#include <math.h>
#include <time.h>

#include "benchmark.h"
#include "logging.h"

inline const struct bmark begin_benchmark(const char *name) {
	long ms;
	struct timespec spec;
	clock_gettime(CLOCK_REALTIME, &spec);
	ms = round(spec.tv_nsec / 1.0e6);

	struct bmark x = {
		.start = ms,
		.name = name
	};

	return x;
}

inline void end_benchmark(const struct bmark x) {
	long ms, diff;
	struct timespec spec;
	clock_gettime(CLOCK_REALTIME, &spec);
	ms = round(spec.tv_nsec / 1.0e6);

	diff = ms - x.start;

	log_msg(LOG_DB, "%s complete in %ims", x.name, diff);
}
