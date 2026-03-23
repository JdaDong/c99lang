/*
 * c99lang - Arena (region-based) memory allocator
 */
#include "util/arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static ArenaBlock *arena_new_block(size_t min_size) {
    size_t sz = min_size > ARENA_BLOCK_SIZE ? min_size : ARENA_BLOCK_SIZE;
    ArenaBlock *b = (ArenaBlock *)malloc(sizeof(ArenaBlock) + sz);
    if (!b) {
        fprintf(stderr, "c99c: out of memory\n");
        abort();
    }
    b->next = NULL;
    b->size = sz;
    b->used = 0;
    return b;
}

void arena_init(Arena *a) {
    a->head = arena_new_block(ARENA_BLOCK_SIZE);
    a->current = a->head;
}

void *arena_alloc(Arena *a, size_t size) {
    /* Align to 8 bytes */
    size = (size + 7) & ~(size_t)7;

    if (a->current->used + size > a->current->size) {
        ArenaBlock *b = arena_new_block(size);
        a->current->next = b;
        a->current = b;
    }
    void *ptr = a->current->data + a->current->used;
    a->current->used += size;
    return ptr;
}

void *arena_calloc(Arena *a, size_t size) {
    void *p = arena_alloc(a, size);
    memset(p, 0, size);
    return p;
}

char *arena_strdup(Arena *a, const char *s) {
    size_t len = strlen(s);
    char *p = (char *)arena_alloc(a, len + 1);
    memcpy(p, s, len + 1);
    return p;
}

char *arena_strndup(Arena *a, const char *s, size_t n) {
    char *p = (char *)arena_alloc(a, n + 1);
    memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

void arena_free(Arena *a) {
    ArenaBlock *b = a->head;
    while (b) {
        ArenaBlock *next = b->next;
        free(b);
        b = next;
    }
    a->head = NULL;
    a->current = NULL;
}
