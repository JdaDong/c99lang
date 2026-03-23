/*
 * c99lang - A C99 Compiler
 * Arena (region-based) memory allocator
 */
#ifndef C99_ARENA_H
#define C99_ARENA_H

#include <stddef.h>

/* Default block size: 64 KiB */
#define ARENA_BLOCK_SIZE (64 * 1024)

typedef struct ArenaBlock {
    struct ArenaBlock *next;
    size_t size;
    size_t used;
    char data[];   /* flexible array member (C99) */
} ArenaBlock;

typedef struct Arena {
    ArenaBlock *head;
    ArenaBlock *current;
} Arena;

void  arena_init(Arena *a);
void *arena_alloc(Arena *a, size_t size);
void *arena_calloc(Arena *a, size_t size);
char *arena_strdup(Arena *a, const char *s);
char *arena_strndup(Arena *a, const char *s, size_t n);
void  arena_free(Arena *a);

#endif /* C99_ARENA_H */
