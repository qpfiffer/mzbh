// vim: noet ts=4 sw=4
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"
#include "models.h"

int webm_serialization() {
	webm to_test = {
		.file_hash = "47C81F590352D9F95C4CDC4FC7985195D5BD45317728A61791F1D9D9EE530E15",
		._null_term_hax_1 = 0,
		.filename = "walrus_eating_cake.webm",
		._null_term_hax_2 = 0,
		.board = "b",
		._null_term_hax_3 = 0,
		.created_at = 1419626658,
		.size = 123000
	};

	char *serialized = serialize_webm(&to_test);
	webm *deserialized = deserialize_webm(serialized);

	assert(memcmp(&to_test, deserialized, sizeof(webm)) == 0);
	free(serialized);
	free(deserialized);

	return 1;
}

int webm_alias_serialization() {
	webm_alias to_test = {
		.file_hash = "47C81F590352D9F95C4CDC4FC7985195D5BD45317728A61791F1D9D9EE530E15",
		._null_term_hax_1 = 0,
		.filename = "walrus_eating_cake.webm",
		._null_term_hax_2 = 0,
		.board = "b",
		._null_term_hax_3 = 0,
		.created_at = 1419626658,
	};

	char *serialized = serialize_alias(&to_test);
	webm_alias *deserialized = deserialize_alias(serialized);

	assert(memcmp(&to_test, deserialized, sizeof(webm_alias)) == 0);
	free(serialized);
	free(deserialized);

	return 1;
}

int hash_stuff() {
	char outbuf[HASH_IMAGE_STR_SIZE] = {0};
	char hash_key[MAX_KEY_SIZE] = "3000655_blitzbang2.webm";

	char outbuf1[HASH_IMAGE_STR_SIZE] = {0};
	char hash_key1[MAX_KEY_SIZE] = "3000655_blitzbang3.webm";

	assert(hash_string_fnv1a((unsigned char *)hash_key, strlen(hash_key), outbuf));
	assert(hash_string_fnv1a((unsigned char *)hash_key1, strlen(hash_key1), outbuf1));

	assert(strcmp(outbuf, outbuf1) != 0);
	return 1;
}

int run_tests() {
	webm_serialization();
	webm_alias_serialization();
	hash_stuff();
	return 0;
}

int main(const int argc, const char *argv[]) {
	return run_tests();
}
