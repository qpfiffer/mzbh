// vim: noet ts=4 sw=4
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>

#include "greshunkel.h"

static const char variable_regex[] = "xXx (?P<variable>[a-zA-Z_0-9\\$]+) xXx";
static const char loop_regex[] = "xXx LOOP (?P<variable>[a-zA-S_]+) (?P<iter_list>[a-zA-S_\\$]+) xXx(?P<subloop>.*)xXx BBL xXx";

greshunkel_ctext *gshkl_init_context() {
	greshunkel_ctext *ctext = calloc(1, sizeof(greshunkel_ctext));
	ctext->values = calloc(1, sizeof(ol_stack));
	return ctext;
}

int gshkl_add_string(greshunkel_ctext *ctext, const char name[WISDOM_OF_WORDS], const char value[MAX_GSHKL_STR_SIZE]) {
	assert(ctext != NULL);

	/* Create a new tuple to hold type and name and shit. */
	greshunkel_tuple _stack_tuple = {
		.name = {0},
		.type = GSHKL_STR,
		.value =  {0}
	};
	strncpy(_stack_tuple.name, name, WISDOM_OF_WORDS);

	/* Copy the value of the string into the var object: */
	greshunkel_var _stack_var = {0};
	strncpy(_stack_var.str, value, MAX_GSHKL_STR_SIZE);

	/* Copy the var object itself into the tuple's var space: */
	memcpy(&_stack_tuple.value, &_stack_var, sizeof(greshunkel_var));

	/* Copy the stack allocated object onto the heap (so we can have const stuff) */
	greshunkel_tuple *new_tuple = calloc(1, sizeof(greshunkel_tuple));
	memcpy(new_tuple, &_stack_tuple, sizeof(greshunkel_tuple));

	/* Push that onto our values stack. */
	spush(&ctext->values, new_tuple);
	return 0;
}

int gshkl_add_num(greshunkel_ctext *ctext, const char name[WISDOM_OF_WORDS], const int value) {
	assert(ctext != NULL);
	return 0;
}

int gshkl_free_context(greshunkel_ctext *ctext) {
	while (ctext->values->next != NULL) {
		greshunkel_tuple *next = (greshunkel_tuple *)spop(&ctext->values);
		free(next);
	}
	free(ctext->values);
	free(ctext);
	return 0;
}

char *gshkl_render(const greshunkel_ctext *ctext, const char *to_render, const size_t original_size, size_t *outsize) {
	assert(to_render != NULL);
	assert(ctext != NULL);

	/* We start up a new buffer and copy the old one into it: */
	char *rendered = calloc(1, original_size);
	if (rendered == NULL)
		goto error;
	memcpy(rendered, to_render, original_size);
	*outsize = original_size;

	/* Variable rendering pass: */

	regex_t regex;
	int reti = regcomp(&regex, variable_regex, 0);
	assert(reti == 0);

	regmatch_t match[1];
	int matches = 0;
	while (regexec(&regex, to_render, 1, match, 0) == 0) {
		/* We matched. */
		matches++;
	}
	return rendered;

error:
	free(rendered);
	*outsize = 0;
	return NULL;
}
