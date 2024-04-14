#ifndef NOREC_VECTOR_H
#define NOREC_VECTOR_H 1

#include "types.h"

/* user-defined value datatype */
#define TDATA struct tx_act

typedef struct vector {
    TDATA *arr;
    int length;
    int capacity;
} vector_t;

vector_t *vector_init(int init_size);

void vector_push_back(vector_t *vec, TDATA *pval);

void vector_destroy(vector_t *vec);

#endif