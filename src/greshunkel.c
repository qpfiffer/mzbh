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

static const char variable_regex[] = "xXx @([a-zA-Z_0-9]+) xXx";
static const char loop_regex[] = "xXx LOOP ([a-zA-Z_]+) ([a-zA-Z_]+) xXx(.*)xXx BBL xXx";

greshunkel_ctext *gshkl_init_context() {
	greshunkel_ctext *ctext = calloc(1, sizeof(greshunkel_ctext));
	ctext->values = calloc(1, sizeof(ol_stack));
	return ctext;
}

static inline int _gshkl_add_var_to_context(greshunkel_ctext *ctext, const greshunkel_tuple *new_tuple) {
	spush(&ctext->values, new_tuple);
	return 0;
}

static inline int _gshkl_add_var_to_loop(greshunkel_var *loop, const greshunkel_tuple *new_tuple) {
	int i, added_to_arr = 0;
	const size_t size = sizeof(loop->arr)/sizeof(loop->arr[0]);
	for (i = 0; i < size; i++) {
		if (loop->arr[i] == NULL) {
			loop->arr[i] = new_tuple;
			added_to_arr = 1;
			break;
		}
	}

	/* Couldn't add to the loop. */
	if (added_to_arr == 0)
		return 1;

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

greshunkel_var *gshkl_add_array(greshunkel_ctext *ctext, const char name[WISDOM_OF_WORDS]) {
	assert(ctext != NULL);

	greshunkel_tuple _stack_tuple = {
		.name = {0},
		.type = GSHKL_ARR,
		.value = {0}
	};
	strncpy(_stack_tuple.name, name, WISDOM_OF_WORDS);

	greshunkel_var _stack_var = {0};

	memcpy(&_stack_tuple.value, &_stack_var, sizeof(greshunkel_var));

	greshunkel_tuple *new_tuple = calloc(1, sizeof(greshunkel_tuple));
	memcpy(new_tuple, &_stack_tuple, sizeof(greshunkel_tuple));

	if (_gshkl_add_var_to_context(ctext, new_tuple) != 0) {
		free(new_tuple);
		return NULL;
	}

	return &new_tuple->value;
}

int gshkl_add_int_to_loop(greshunkel_var *loop, const int value) {
	assert(loop != NULL);

	greshunkel_tuple _stack_tuple = {
		.name = {0},
		.type = GSHKL_STR,
		.value = {0}
	};

	greshunkel_var _stack_var = {0};
	snprintf(_stack_var.str, MAX_GSHKL_STR_SIZE, "%i", value);

	memcpy(&_stack_tuple.value, &_stack_var, sizeof(greshunkel_var));

	greshunkel_tuple *new_tuple = calloc(1, sizeof(greshunkel_tuple));
	memcpy(new_tuple, &_stack_tuple, sizeof(greshunkel_tuple));

	if (_gshkl_add_var_to_loop(loop, new_tuple) != 0) {
		free(new_tuple);
		return 1;
	}

	return 0;
}

int gshkl_add_string_to_loop(greshunkel_var *loop, const char *value) {
	assert(loop != NULL);

	greshunkel_tuple _stack_tuple = {
		.name = {0},
		.type = GSHKL_STR,
		.value = {0}
	};

	greshunkel_var _stack_var = {0};
	strncpy(_stack_var.str, value, MAX_GSHKL_STR_SIZE);

	memcpy(&_stack_tuple.value, &_stack_var, sizeof(greshunkel_var));

	greshunkel_tuple *new_tuple = calloc(1, sizeof(greshunkel_tuple));
	memcpy(new_tuple, &_stack_tuple, sizeof(greshunkel_tuple));

	if (_gshkl_add_var_to_loop(loop, new_tuple) != 0) {
		free(new_tuple);
		return 1;
	}

	return 0;
}

static void _gshkl_free_arr(greshunkel_tuple *to_free) {
	assert(to_free->type == GSHKL_ARR);

	int i;
	const size_t size = sizeof(to_free->value.arr)/sizeof(to_free->value.arr[0]);
	for (i = 0; i < size; i++) {
		if (to_free->value.arr[i] == NULL)
			break;
		free((struct greshunkel_tuple *)to_free->value.arr[i]);
		to_free->value.arr[i] = NULL;
	}
	free(to_free);
}

int gshkl_free_context(greshunkel_ctext *ctext) {
	while (ctext->values->next != NULL) {
		greshunkel_tuple *next = (greshunkel_tuple *)spop(&ctext->values);
		if (next->type == GSHKL_ARR) {
			_gshkl_free_arr(next);
			continue;
		}
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

static line
_interpolate_line(const greshunkel_ctext *ctext, const line current_line, const regex_t *var_regex) {
	line interpolated_line = {0};
	line new_line_to_add = {0};
	regmatch_t match[2];
	const line *operating_line = &current_line;

	while (regexec(var_regex, operating_line->data, 2, match, 0) == 0) {
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

			assert(tuple->name != NULL);
			int strcmp_result = strncmp(tuple->name, operating_line->data + inner_match.rm_so, strlen(tuple->name));
			if (tuple->type == GSHKL_STR && strcmp_result == 0) {
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

		free(interpolated_line.data);
		interpolated_line.size = new_line_to_add.size;
		interpolated_line.data = new_line_to_add.data;
		new_line_to_add.size = 0;
		new_line_to_add.data = NULL;
		operating_line = &interpolated_line;

		/* Set the next regex check after this one. */
		memset(match, 0, sizeof(match));
	}
	if (interpolated_line.data != NULL)
		return interpolated_line;
	line _to_return = { .size = current_line.size, .data = calloc(1, current_line.size)};
	memcpy(_to_return.data, current_line.data, current_line.size);
	return _to_return;
}

static inline void _compile_regex(regex_t *vr, regex_t *lr) {
	int reti = regcomp(vr, variable_regex, REG_EXTENDED);
	assert(reti == 0);

	reti = regcomp(lr, loop_regex, REG_EXTENDED);
	assert(reti == 0);
}

static inline void _destroy_regex(regex_t *vr, regex_t *lr) {
	regfree(vr);
	regfree(lr);
}

char *gshkl_render(const greshunkel_ctext *ctext, const char *to_render, const size_t original_size, size_t *outsize) {
	assert(to_render != NULL);
	assert(ctext != NULL);

	/* We start up a new buffer and copy the old one into it: */
	char *rendered = NULL;
	*outsize = 0;

	regex_t var_regex, loop_regex;
	_compile_regex(&var_regex, &loop_regex);

	size_t num_read = 0;
	while (num_read < original_size) {
		line current_line = read_line(to_render + num_read);
		num_read += current_line.size;

		line to_append = {0};
		to_append = _interpolate_line(ctext, current_line, &var_regex);

		/* Fuck this */
		const size_t old_outsize = *outsize;
		*outsize += to_append.size;
		{
			char *med_buf = realloc(rendered, *outsize);
			if (med_buf == NULL)
				goto error;
			rendered = med_buf;
		}
		strncpy(rendered + old_outsize, to_append.data, to_append.size);
		free(current_line.data);
		free(to_append.data);
	}
	_destroy_regex(&var_regex, &loop_regex);
	return rendered;

error:
	free(rendered);
	*outsize = 0;
	return NULL;
}
