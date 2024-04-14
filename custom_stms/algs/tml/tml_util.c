#include <stdlib.h>
#include "tml_util.h"

#define VEC_INIT 8

int tx_stack_init(struct tx_stack **sp)
{
	if (*sp != NULL)
		return 0;

	struct tx_stack *e = malloc(sizeof(struct tx_stack));
	if (e == NULL)
		return 1;

	e->first = NULL;
	e->head = NULL;
	*sp = e;
	return 0;
}

void tx_stack_push(struct tx_stack *s, struct tx_data *entry)
{
	if (s->head == NULL) {
		s->head = entry;
		s->first = entry;
	} else {
		entry->next = s->head;
		s->head = entry;
	}
}

struct tx_data *tx_stack_pop(struct tx_stack *s)
{
	struct tx_data *head = s->head;
	if (head == NULL) 
		return NULL;

	if (head->next == NULL) {
		s->first = NULL;
		s->head = NULL;
	} else {
		s->head = head->next;
	}

	head->next = NULL;
	return head;
}

void tx_stack_empty(struct tx_stack *s)
{
	struct tx_data *var;
	while (s->head != NULL) {
		var = s->head;
		s->head = var->next;
		free(var);
	}
	s->first = NULL;
}

int tx_stack_destroy(struct tx_stack **sp)
{
	if (*sp == NULL)
		return 1;

	tx_stack_empty(*sp);
	free(*sp);
	*sp = NULL;

	return 0;
}

int tx_stack_isempty(struct tx_stack *s)
{
	return s->first == NULL && s->head == NULL;
}

int tx_vector_init(struct tx_vec **vecp)
{
	struct tx_vec *v = malloc(sizeof(struct tx_vec));
	if (v == NULL)
		return 1;

	v->length = 0;
	v->capacity = VEC_INIT;

	size_t sz = VEC_INIT * sizeof(void *);
	v->addrs = malloc(sz);
	memset(v->addrs, 0, sz);
	*vecp = v;

	return 0;
}

int tx_vector_resize(struct tx_vec *vec)
{
	size_t new_cap = vec->capacity * 2;
	size_t sz = new_cap * sizeof(void *);
	void **tmp = malloc(sz);
	if (tmp == NULL)
		return 1;

	memset(tmp, 0, sz);
	memcpy(tmp, vec->addrs, vec->capacity * sizeof(void *));
	free(vec->addrs);
	vec->addrs = tmp;
	vec->capacity = new_cap;

	return 0;
}

int tx_vector_append(struct tx_vec *vec, void *entry)
{
	if (vec->length >= vec->capacity) {
		if (tx_vector_resize(vec)) 
			return 1;
	}

	vec->addrs[vec->length++] = entry;
	return 0;
}

void tx_vector_destroy(struct tx_vec *vec)
{
	free(vec->addrs);
	free(vec);
}

void tx_vector_empty(struct tx_vec *vec)
{
	memset(vec->addrs, 0, vec->capacity * sizeof(void *));
	vec->length = 0;
}