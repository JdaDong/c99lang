/*
 * c99lang - Symbol table implementation
 */
#include "sema/symtab.h"
#include <string.h>

static unsigned sym_hash(const char *name) {
    unsigned h = 5381;
    for (; *name; name++)
        h = ((h << 5) + h) + (unsigned char)*name;
    return h;
}

void symtab_init(SymTab *st, Arena *arena) {
    memset(st->buckets, 0, sizeof(st->buckets));
    st->scope_depth = 0;
    st->arena = arena;
}

void symtab_push_scope(SymTab *st) {
    st->scope_depth++;
}

void symtab_pop_scope(SymTab *st) {
    /* Remove all symbols at current scope depth */
    for (int i = 0; i < SYMTAB_BUCKETS; i++) {
        Symbol **prev = &st->buckets[i];
        Symbol *s = *prev;
        while (s) {
            if (s->scope_depth == st->scope_depth) {
                *prev = s->next;
                s = *prev;
            } else {
                prev = &s->next;
                s = s->next;
            }
        }
    }
    st->scope_depth--;
}

Symbol *symtab_insert(SymTab *st, const char *name, SymKind kind,
                      CType *type, SrcLoc loc) {
    unsigned idx = sym_hash(name) % SYMTAB_BUCKETS;

    Symbol *s = (Symbol *)arena_calloc(st->arena, sizeof(Symbol));
    s->name = name;
    s->kind = kind;
    s->type = type;
    s->loc = loc;
    s->scope_depth = st->scope_depth;
    s->linkage = LINK_NONE;
    s->is_defined = false;
    s->ir_slot = -1;
    s->next = st->buckets[idx];
    st->buckets[idx] = s;

    return s;
}

Symbol *symtab_lookup(SymTab *st, const char *name) {
    unsigned idx = sym_hash(name) % SYMTAB_BUCKETS;
    for (Symbol *s = st->buckets[idx]; s; s = s->next) {
        if (strcmp(s->name, name) == 0) return s;
    }
    return NULL;
}

Symbol *symtab_lookup_current_scope(SymTab *st, const char *name) {
    unsigned idx = sym_hash(name) % SYMTAB_BUCKETS;
    for (Symbol *s = st->buckets[idx]; s; s = s->next) {
        if (s->scope_depth == st->scope_depth &&
            strcmp(s->name, name) == 0) return s;
    }
    return NULL;
}
