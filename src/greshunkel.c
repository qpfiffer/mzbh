// vim: noet ts=4 sw=4
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>

#include "greshunkel.h"

struct line {
	size_t size;
	char *data;
};

typedef struct line line;

static const char variable_regex[] = "xXx @([a-zA-Z_0-9\\$]+) xXx";
static const char loop_regex[] = "xXx LOOP (?P<variable>[a-zA-S_]+) (?P<iter_list>[a-zA-S_\\$]+) xXx(?P<subloop>.*)xXx BBL xXx";

greshunkel_ctext *gshkl_init_context() {
	greshunkel_ctext *ctext = calloc(1, sizeof(greshunkel_ctext));
	ctext->values = calloc(1, sizeof(ol_stack));
	return ctext;
}

static inline int _gshkl_add_var_to_context(greshunkel_ctext *ctext, const greshunkel_tuple *new_tuple) {
	spush(&ctext->values, new_tuple);
	return 0;
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
	if (_gshkl_add_var_to_context(ctext, new_tuple) != 0) {
		free(new_tuple);
		return 1;
	}
	return 0;
}

int gshkl_add_int(greshunkel_ctext *ctext, const char name[WISDOM_OF_WORDS], const int value) {
	assert(ctext != NULL);

	greshunkel_tuple _stack_tuple = {
		.name = {0},
		.type = GSHKL_STR,
		.value = {0}
	};
	strncpy(_stack_tuple.name, name, WISDOM_OF_WORDS);

	greshunkel_var _stack_var = {0};
	snprintf(_stack_var.str, MAX_GSHKL_STR_SIZE, "%i", value);

	memcpy(&_stack_tuple.value, &_stack_var, sizeof(greshunkel_var));

	greshunkel_tuple *new_tuple = calloc(1, sizeof(greshunkel_tuple));
	memcpy(new_tuple, &_stack_tuple, sizeof(greshunkel_tuple));

	if (_gshkl_add_var_to_context(ctext, new_tuple) != 0) {
		free(new_tuple);
		return 1;
	}
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

static line read_line(const char *buf) {
	char c = '\0';

	size_t num_read = 0;
	while (1) {
		c = buf[num_read];
		num_read++;
		if (c == '\0' || c == '\n' || c == '\r')
			break;
	}

	line to_return = {
		.size = num_read,
		.data = calloc(1, num_read + 1)
	};
	strncpy(to_return.data, buf, num_read);

	return to_return;
}

char *gshkl_render(const greshunkel_ctext *ctext, const char *to_render, const size_t original_size, size_t *outsize) {
	assert(to_render != NULL);
	assert(ctext != NULL);

	/* We start up a new buffer and copy the old one into it: */
	char *rendered = NULL;
	*outsize = 0;

	regex_t regex;
	int reti = regcomp(&regex, variable_regex, REG_EXTENDED);
#ifdef DEBUG
	if (reti != 0) {
		char errbuf[128];
		regerror(reti, &regex, errbuf, sizeof(errbuf));
		printf("%s\n", errbuf);
#endif
		assert(reti == 0);
#ifdef DEBUG
	}
#endif

	size_t num_read = 0;
	while (num_read < original_size) {
		line current_line = read_line(to_render + num_read);
		/* Variable rendering pass: */

		line line_to_add = {0};
		line new_line_to_add = {0};
		regmatch_t match[2];
		line *operating_line = &current_line;

		while (regexec(&regex, operating_line->data, 2, match, 0) == 0) {
			int matched_at_least_once = 0;
			/* We matched. */
			ol_stack *current_value = ctext->values;

			/* We linearly search through our variables because I don't have
			 * a hash map. C is "fast enough" */
			while (current_value->next != NULL) {
				const greshunkel_tuple *tuple = (greshunkel_tuple *)current_value->data;
				/* This is the actual part of the regex we care about. */
				const regmatch_t inner_match = match[1];
				assert(inner_match.rm_so != -1 && inner_match.rm_eo != -1);

				if (strncmp(tuple->name, operating_line->data + inner_match.rm_so, strlen(tuple->name)) == 0) {
#ifdef DEBUG
					printf("Hit a set variable!\n");
#endif
					/* Do actual printing here */
					const size_t first_piece_size = match[0].rm_so;
					const size_t middle_piece_size = strlen(tuple->value.str);
					const size_t last_piece_size = operating_line->size - match[0].rm_eo;
					/* Sorry, Vishnu... */
					new_line_to_add.size = first_piece_size + middle_piece_size + last_piece_size;
					new_line_to_add.data = calloc(1, new_line_to_add.size);

					strncpy(new_line_to_add.data, operating_line->data, first_piece_size);
					/* TODO: DO NOT ASSUME IT IS ALWAYS A STRING! */
					strncpy(new_line_to_add.data + first_piece_size, tuple->value.str, middle_piece_size);
					strncpy(new_line_to_add.data + first_piece_size + middle_piece_size,
							operating_line->data + match[0].rm_eo,
							last_piece_size);

					matched_at_least_once = 1;
					break;
				}
				current_value = current_value->next;
			}
			/* Blow up if we had a variable that wasn't in the context. */
			assert(matched_at_least_once = 1);

			free(line_to_add.data);
			line_to_add.size = new_line_to_add.size;
			line_to_add.data = new_line_to_add.data;
			new_line_to_add.size = 0;
			new_line_to_add.data = NULL;
			operating_line = &line_to_add;

			/* Set the next regex check after this one. */
			memset(match, 0, sizeof(match));
		}
		num_read += current_line.size;

		/* Fuck this */
#define MAKE_BUFFER if (rendered == NULL) {\
				rendered = calloc(1, *outsize);\
			} else {\
				char *med_buf = realloc(rendered, *outsize);\
				if (med_buf == NULL)\
					goto error;\
				rendered = med_buf;\
			}

		const size_t old_outsize = *outsize;
		if (line_to_add.size != 0) {
			*outsize += line_to_add.size;
			MAKE_BUFFER
			strncpy(rendered + old_outsize, line_to_add.data, line_to_add.size);
			free(current_line.data);
			free(line_to_add.data);
		} else {
			*outsize += current_line.size;
			MAKE_BUFFER
			strncpy(rendered + old_outsize, current_line.data, current_line.size);
			free(current_line.data);
		}
	}
	regfree(&regex);
	return rendered;

error:
	free(rendered);
	*outsize = 0;
	return NULL;
}
