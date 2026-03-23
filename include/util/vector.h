/*
 * c99lang - A C99 Compiler
 * Generic dynamic array (type-unsafe void* vector)
 */
#ifndef C99_VECTOR_H
#define C99_VECTOR_H

#include <stddef.h>

typedef struct Vector {
    void **data;
    size_t size;
    size_t cap;
} Vector;

void  vec_init(Vector *v);
void  vec_free(Vector *v);
void  vec_push(Vector *v, void *item);
void *vec_pop(Vector *v);
void *vec_get(const Vector *v, size_t idx);
void  vec_set(Vector *v, size_t idx, void *item);
void  vec_clear(Vector *v);

#endif /* C99_VECTOR_H */
