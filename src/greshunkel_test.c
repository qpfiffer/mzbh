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

	char *rendered = gshkl_render(ctext, document, strlen(document), &new_size);
	gshkl_free_context(ctext);

	printf("%s\n", rendered);
	free(rendered);

	return 0;
}
