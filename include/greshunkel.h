// vim: noet ts=4 sw=4
#pragma once

/* AND HE DECREED: */
/* STRINGS SHALL NEVER BE MORE THAN 32 OCTETS! */
/* ARRAYS OF MORE VARIABLES WILL NEVER BE MORE THAN 8 ITEMS IN LENGTH! */
/* NUMBERS SHALL NEVER BE GREATER THAN THE VALUE OF A SINGLE INTEGER! */
typedef union greshunkel_var {
	const int num;
	const char str[32];
	const greshunkel_var arr[8];
} greshunkel_var;

typedef enum {
	GSHKL_NUM,
	GSHKL_ARR,
	GSHKL_STR
} greshunkel_type;

typedef struct {
	const char *name;
	const greshunkel_type type;
	const greshunkel_var *value;
} greshunkel_tuple;

typedef struct {
	greshunkel_tuple *values;
} greshunkel_ctext;

int gshkl_init_context(greshunkel_ctext *ctext);
int gshkl_free_context(greshunkel_ctext *ctext);
