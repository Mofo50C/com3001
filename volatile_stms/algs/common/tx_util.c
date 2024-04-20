#include <stdlib.h>
#include <string.h>

#include <util.h>
#include "tx_util.h"

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
		return 0;

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

	size_t sz = VEC_INIT * sizeof(struct tx_vec_entry);
	v->arr = malloc(sz);
	if (v->arr == NULL) {
		free(v);
		return 1;
	}

	memset(v->arr, 0, sz);
	*vecp = v;
	return 0;
}

int tx_vector_resize(struct tx_vec *vec)
{
	size_t new_cap = vec->capacity * 2;
	size_t sz = new_cap * sizeof(struct tx_vec_entry);
	struct tx_vec_entry *tmp = malloc(sz);
	if (tmp == NULL)
		return 1;

	memset(tmp, 0, sz);
	memcpy(tmp, vec->arr, vec->capacity * sizeof(struct tx_vec_entry));
	free(vec->arr);
	vec->arr = tmp;
	vec->capacity = new_cap;

	return 0;
}

/* returns -1 on error */
int tx_vector_append(struct tx_vec *vec, struct tx_vec_entry *entry)
{
	if (vec->length >= vec->capacity) {
		if (tx_vector_resize(vec)) 
			return -1;
	}

	int idx = vec->length++;
	vec->arr[idx] = *entry;
	return idx;
}


void tx_vector_destroy(struct tx_vec **vecp)
{
	struct tx_vec *vec = *vecp;
	free(vec->arr);
	free(vec);
	*vecp = NULL;
}

void _vector_free_entries(struct tx_vec *vec)
{
	struct tx_vec_entry *entry;
	int i;
	for (i = 0; i < vec->length; i++) {
		entry = &vec->arr[i];
		free(entry->pval);
		entry->pval = NULL;
	}
}

void tx_vector_empty(struct tx_vec *vec)
{
	_vector_free_entries(vec);
	memset(vec->arr, 0, vec->capacity * sizeof(void *));
	vec->length = 0;
}

void tx_vector_empty_unsafe(struct tx_vec *vec)
{
	memset(vec->arr, 0, vec->capacity * sizeof(void *));
	vec->length = 0;
}

int tx_util_is_zeroed(const void *addr, size_t len)
{
	const char *a = addr;

	if (len == 0)
		return 1;

	if (a[0] == 0 && memcmp(a, a + 1, len - 1) == 0)
		return 1;

	return 0;
}