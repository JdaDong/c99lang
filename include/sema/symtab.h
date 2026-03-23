/*
 * c99lang - A C99 Compiler
 * Symbol table
 */
#ifndef C99_SYMTAB_H
#define C99_SYMTAB_H

#include "sema/type.h"
#include "util/error.h"

typedef enum SymKind {
    SYM_VAR,
    SYM_FUNC,
    SYM_TYPEDEF,
    SYM_ENUM_CONST,
    SYM_PARAM,
    SYM_LABEL,
} SymKind;

typedef enum SymLinkage {
    LINK_NONE,
    LINK_INTERNAL,
    LINK_EXTERNAL,
} SymLinkage;

typedef struct Symbol {
    const char *name;
    SymKind kind;
    CType *type;
    SymLinkage linkage;
    SrcLoc loc;
    int scope_depth;
    bool is_defined;      /* for functions: has body? */

    /* For enum constants */
    long long enum_value;

    /* For IR: storage slot / register */
    int ir_slot;

    struct Symbol *next;  /* hash chain */
} Symbol;

#define SYMTAB_BUCKETS 256

typedef struct SymTab {
    Symbol *buckets[SYMTAB_BUCKETS];
    int scope_depth;
    Arena *arena;
} SymTab;

void    symtab_init(SymTab *st, Arena *arena);
void    symtab_push_scope(SymTab *st);
void    symtab_pop_scope(SymTab *st);
Symbol *symtab_insert(SymTab *st, const char *name, SymKind kind,
                      CType *type, SrcLoc loc);
Symbol *symtab_lookup(SymTab *st, const char *name);
Symbol *symtab_lookup_current_scope(SymTab *st, const char *name);

#endif /* C99_SYMTAB_H */
