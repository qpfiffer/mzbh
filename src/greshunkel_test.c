// vim: noet ts=4 sw=4
#include <stdio.h>
#include <string.h>
#include "greshunkel.h"

const char document[] =
"<html>\n"
"	<body>\n"
"		<p>xXx TEST xXx</p>\n"
"	</body>\n"
"</html>\n";

int main(int argc, char *argv[]) {
	size_t new_size = 0;
	greshunkel_ctext *ctext = gshkl_init_context();
	const char *rendered = gshkl_render(ctext, document, strlen(document), &new_size);
	printf("%s\n", rendered);
	gshkl_free_context(ctext);

	return 0;
}
