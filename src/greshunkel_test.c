// vim: noet ts=4 sw=4
#include <stdio.h>
#include <string.h>
#define DEBUG
#include "greshunkel.h"

const char document[] =
"<html>\n"
"	<body>\n"
"		<span>This is the real xXx @TRICKY xXx xXx @ONE xXx</span>\n"
"		<p>This is a regular string: xXx @TEST xXx</p>\n"
"		<p>This is an integer: xXx @FAKEINT xXx</p>\n"
"		<ul>\n"
"		xXx LOOP i LOOP_TEST xXx\n"
"			<li>xXx i xXx</li>\n"
"		xXx BBL xXx\n"
"		</ul>\n"
"	</body>\n"
"</html>\n";

int main(int argc, char *argv[]) {
	size_t new_size = 0;

	greshunkel_ctext *ctext = gshkl_init_context();

	gshkl_add_string(ctext, "TEST", "This is a test.");
	gshkl_add_int(ctext, "FAKEINT", 666);

	gshkl_add_string(ctext, "TRICKY", "TrIcKy");
	gshkl_add_int(ctext, "ONE", 1);

	greshunkel_var *loop_test = gshkl_add_array(ctext, "LOOP_TEST");

	gshkl_add_string_to_loop(loop_test, "a");
	gshkl_add_string_to_loop(loop_test, "b");
	gshkl_add_string_to_loop(loop_test, "c");

	gshkl_add_int_to_loop(loop_test, 1);
	gshkl_add_int_to_loop(loop_test, 2);
	gshkl_add_int_to_loop(loop_test, 3);

	char *rendered = gshkl_render(ctext, document, strlen(document), &new_size);
	gshkl_free_context(ctext);

	printf("%s\n", rendered);
	free(rendered);

	return 0;
}
