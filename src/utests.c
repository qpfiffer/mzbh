// vim: noet ts=4 sw=4
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "utests.h"
#include "models.h"

int webm_serialization() {
	webm to_test = {
		.file_hash = "47C81F590352D9F95C4CDC4FC7985195D5BD45317728A61791F1D9D9EE530E15",
		.filename = "walrus_eating_cake.webm",
		.board = "b",
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
		.filename = "walrus_eating_cake.webm",
		.board = "b",
		.created_at = 1419626658,
	};

	char *serialized = serialize_alias(&to_test);
	webm_alias *deserialized = deserialize_alias(serialized);

	assert(memcmp(&to_test, deserialized, sizeof(webm_alias)) == 0);
	free(serialized);
	free(deserialized);

	return 1;
}

int run_tests() {
	webm_serialization();
	webm_alias_serialization();
	return 0;
}

int main(const int argc, const char *argv[]) {
	return run_tests();
}
