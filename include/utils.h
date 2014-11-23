// vim: noet ts=4 sw=4
#pragma once
#include <stdlib.h>
#include <math.h>

#define INT_LEN(x) floor(log10(abs(x))) + 1

/* Doesn't actually fully decode URLs at this point. */
void url_decode(const char *src, const size_t src_siz, char *dest);

/* String helper stuff. */
int endswith(const char *string, const char *suffix);
char *strnstr(const char *haystack, const char *needle, size_t len);

/* Constant variables in global space. Woooo! */
const char *webm_location();
const char *db_location();
