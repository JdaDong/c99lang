/*
 * c99lang - Generic dynamic array
 */
#include "util/vector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define VEC_INIT_CAP 8

void vec_init(Vector *v) {
    v->data = NULL;
    v->size = 0;
    v->cap = 0;
}

void vec_free(Vector *v) {
    free(v->data);
    v->data = NULL;
    v->size = 0;
    v->cap = 0;
}

void vec_push(Vector *v, void *item) {
    if (v->size >= v->cap) {
        v->cap = v->cap == 0 ? VEC_INIT_CAP : v->cap * 2;
        v->data = (void **)realloc(v->data, v->cap * sizeof(void *));
        if (!v->data) {
            fprintf(stderr, "c99c: out of memory\n");
            abort();
        }
    }
    v->data[v->size++] = item;
}

void *vec_pop(Vector *v) {
    assert(v->size > 0);
    return v->data[--v->size];
}

void *vec_get(const Vector *v, size_t idx) {
    assert(idx < v->size);
    return v->data[idx];
}

void vec_set(Vector *v, size_t idx, void *item) {
    assert(idx < v->size);
    v->data[idx] = item;
}

void vec_clear(Vector *v) {
    v->size = 0;
}
