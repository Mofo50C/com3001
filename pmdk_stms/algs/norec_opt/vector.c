#include <stdlib.h>
#include "vector.h"

vector_t *vector_init(int init_size)
{
	vector_t *v = malloc(sizeof(vector_t));
	v->length = 0;
	v->capacity = init_size;
	v->arr = malloc(init_size * sizeof(TDATA));

	return v;
}

void vector_resize(vector_t *vec)
{
	vec->capacity *= 2;
	vec->arr = realloc(vec->arr, vec->capacity * sizeof(TDATA));
}

void vector_push_back(vector_t *vec, TDATA *pval)
{
	if (vec->length >= vec->capacity)
		vector_resize(vec);

	vec->arr[vec->length++] = *pval;
}

void vector_destroy(vector_t *vec)
{
	free(vec->arr);
	free(vec);
}

