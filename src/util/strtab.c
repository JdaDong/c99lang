/*
 * c99lang - String table (string interning)
 */
#include "util/strtab.h"
#include <string.h>
#include <stdlib.h>

static unsigned strtab_hash(const char *s, size_t len) {
    unsigned h = 5381;
    for (size_t i = 0; i < len; i++) {
        h = ((h << 5) + h) + (unsigned char)s[i];
    }
    return h;
}

void strtab_init(Strtab *st) {
    arena_init(&st->arena);
    memset(st->buckets, 0, sizeof(st->buckets));
    st->count = 0;
}

void strtab_free(Strtab *st) {
    arena_free(&st->arena);
    memset(st->buckets, 0, sizeof(st->buckets));
    st->count = 0;
}

const char *strtab_intern(Strtab *st, const char *s, size_t len) {
    unsigned h = strtab_hash(s, len);
    unsigned idx = h % STRTAB_BUCKET_COUNT;

    /* Search existing entries */
    for (StrtabEntry *e = st->buckets[idx]; e; e = e->next) {
        if (e->hash == h && e->len == len && memcmp(e->str, s, len) == 0) {
            return e->str;
        }
    }

    /* Insert new entry */
    char *interned = arena_strndup(&st->arena, s, len);
    StrtabEntry *e = (StrtabEntry *)arena_alloc(&st->arena, sizeof(StrtabEntry));
    e->str = interned;
    e->len = len;
    e->hash = h;
    e->next = st->buckets[idx];
    st->buckets[idx] = e;
    st->count++;

    return interned;
}

const char *strtab_intern_cstr(Strtab *st, const char *s) {
    return strtab_intern(st, s, strlen(s));
}
