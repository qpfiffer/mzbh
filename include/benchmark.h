// vim: noet ts=4 sw=4
#pragma once

struct bmark {
	long start;
	const char *name;
};

const struct bmark begin_benchmark(const char *name);
void end_benchmark(const struct bmark x);
