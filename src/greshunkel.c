// vim: noet ts=4 sw=4
#include <stdlib.h>
#include "greshunkel.h"

int gshkl_init_context(greshunkel_ctext *ctext) {
	ctext = calloc(1, sizeof(greshunkel_ctext));
	return 0;
}

int gshkl_free_context(greshunkel_ctext *ctext) {
	free(ctext);
	return 0;
}
