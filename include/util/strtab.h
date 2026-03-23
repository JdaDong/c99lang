/*
 * c99lang - A C99 Compiler
 * String table (string interning)
 */
#ifndef C99_STRTAB_H
#define C99_STRTAB_H

#include <stddef.h>
#include "util/arena.h"

#define STRTAB_BUCKET_COUNT 1024

typedef struct StrtabEntry {
    struct StrtabEntry *next;
    const char *str;
    size_t len;
    unsigned hash;
} StrtabEntry;

typedef struct Strtab {
    Arena arena;
    StrtabEntry *buckets[STRTAB_BUCKET_COUNT];
    size_t count;
} Strtab;

void        strtab_init(Strtab *st);
void        strtab_free(Strtab *st);
const char *strtab_intern(Strtab *st, const char *s, size_t len);
const char *strtab_intern_cstr(Strtab *st, const char *s);

#endif /* C99_STRTAB_H */
