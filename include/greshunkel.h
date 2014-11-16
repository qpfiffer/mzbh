// vim: noet ts=4 sw=4
#pragma once
#include <stdlib.h>
#include "stack.h"

#define WISDOM_OF_WORDS 32
#define MAX_GSHKL_STR_SIZE 32

/* AND HE DECREED: */
/* STRINGS SHALL NEVER BE MORE THAN 32 OCTETS! */
/* ARRAYS OF MORE VARIABLES WILL NEVER BE MORE THAN 8 ITEMS IN LENGTH! */
/* NUMBERS SHALL NEVER BE GREATER THAN THE VALUE OF A SINGLE INTEGER! */
/* VARIABLE NAMES SHALL NEVER BE MORE THAN 32 CHARACTERS! */
typedef union greshunkel_var {
	const unsigned int fuck_gcc : 1; /* This tricks GCC into doing smart things. Not used. */
	char str[MAX_GSHKL_STR_SIZE];
	union greshunkel_var *arr[8];
} greshunkel_var;

typedef enum {
	GSHKL_ARR,
	GSHKL_STR
} greshunkel_type;

typedef struct {
	char name[WISDOM_OF_WORDS];
	const greshunkel_type type;
	greshunkel_var value;
} greshunkel_tuple;

typedef struct {
	ol_stack *values;
} greshunkel_ctext;

/* Build and destroy contexts: */
greshunkel_ctext *gshkl_init_context();
int gshkl_free_context(greshunkel_ctext *ctext);

/* Add things to the contexts: */
int gshkl_add_string(greshunkel_ctext *ctext, const char name[WISDOM_OF_WORDS], const char *value);
int gshkl_add_int(greshunkel_ctext *ctext, const char name[WISDOM_OF_WORDS], const int value);

/* Render a string buffer: */
char *gshkl_render(const greshunkel_ctext *ctext, const char *to_render, const size_t original_size, size_t *outsize);
