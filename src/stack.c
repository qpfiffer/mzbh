// vim: noet ts=4 sw=4
#include <stdlib.h>
#include "stack.h"

inline void spush(ol_stack **stack, const void *data) {
	if (stack == NULL || data == NULL)
		goto error;

	ol_stack *to_push = NULL;
	to_push = malloc(sizeof(ol_stack));
	if (to_push == NULL)
		goto error;

	to_push->data = data;
	to_push->next = *stack;

	*stack = to_push;

error:
	return;
}

inline const void *spop(ol_stack **stack) {
	if (stack == NULL)
		goto error;

	ol_stack *top = *stack;
	*stack = top->next;
	const void *data = top->data;

	free(top);
	return data;

error:
	return NULL;
}
