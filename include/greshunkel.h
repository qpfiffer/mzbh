// vim: noet ts=4 sw=4
#pragma once

#define WISDOM_OF_WORDS 32

/* AND HE DECREED: */
/* STRINGS SHALL NEVER BE MORE THAN 32 OCTETS! */
/* ARRAYS OF MORE VARIABLES WILL NEVER BE MORE THAN 8 ITEMS IN LENGTH! */
/* NUMBERS SHALL NEVER BE GREATER THAN THE VALUE OF A SINGLE INTEGER! */
/* VARIABLE NAMES SHALL NEVER BE MORE THAN 32 CHARACTERS! */
typedef union greshunkel_var {
	const int num;
	const char str[32];
	const union greshunkel_var *arr[8];
} greshunkel_var;

typedef enum {
	GSHKL_NUM,
	GSHKL_ARR,
	GSHKL_STR
} greshunkel_type;

typedef struct {
	const char name[WISDOM_OF_WORDS];
	const greshunkel_type type;
	const greshunkel_var *value;
} greshunkel_tuple;

typedef struct {
	greshunkel_tuple *values;
} greshunkel_ctext;

int gshkl_init_context(greshunkel_ctext *ctext);
int gshkl_add_variable(const char name[WISDOM_OF_WORDS], const greshunkel_tuple *new_var);
int gshkl_free_context(greshunkel_ctext *ctext);
