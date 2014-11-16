// vim: noet ts=4 sw=4
#include <stdio.h>
#include <string.h>
#define DEBUG
#include "greshunkel.h"

const char document[] =
"<html>\n"
"	<body>\n"
"		<p>This is a regular string: xXx @TEST xXx</p>\n"
"		<p>This is an integer: xXx @FAKEINT xXx</p>\n"
"	</body>\n"
"</html>\n";

int main(int argc, char *argv[]) {
	size_t new_size = 0;

	greshunkel_ctext *ctext = gshkl_init_context();

	gshkl_add_string(ctext, "TEST", "This is a test.");
	gshkl_add_int(ctext, "TEST", 666);

	const char *rendered = gshkl_render(ctext, document, strlen(document), &new_size);
	gshkl_free_context(ctext);

	printf("%s\n", rendered);

	return 0;
}
