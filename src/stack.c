// vim: noet ts=4 sw=4
#include <stdlib.h>
#include "stack.h"

inline void spush(ol_stack **stack, const void *data) {
	if (stack == NULL || data == NULL)
		return;

	ol_stack *to_push = NULL;
	to_push = malloc(sizeof(ol_stack));
	if (to_push == NULL)
		return;

	to_push->data = data;
	to_push->next = *stack;

	*stack = to_push;

	return;
}

inline const void *spop(ol_stack **stack) {
	if (stack == NULL)
		return NULL;

	ol_stack *top = *stack;
	*stack = top->next;
	const void *data = top->data;

	free(top);
	return data;
}
