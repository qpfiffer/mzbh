// vim: noet ts=4 sw=4
#ifdef __clang__
	#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#endif

#include <math.h>
#include <time.h>

#include <38-moths/logging.h>

#include "benchmark.h"
#include "utils.h"

inline struct bmark begin_benchmark(const char *name) {
#ifdef BENCHMARK
	long ms;
	struct timespec spec;
	clock_gettime(CLOCK_MONOTONIC, &spec);
	ms = round(spec.tv_nsec / 1.0e6);

	struct bmark x = {
		.start = ms,
		.name = name
	};

	return x;
#else
	UNUSED(name);
	struct bmark x = {0};

	return x;
#endif
}

inline void end_benchmark(const struct bmark x) {
#ifdef BENCHMARK
	long ms, diff;
	struct timespec spec;
	clock_gettime(CLOCK_REALTIME, &spec);
	ms = round(spec.tv_nsec / 1.0e6);

	diff = ms - x.start;

	log_msg(LOG_DB, "%s complete in %ims", x.name, diff);
#else
	UNUSED(x);
#endif
}
