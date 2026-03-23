/*
 * c99lang - A C99 Compiler
 * C99 Preprocessor
 */
#ifndef C99_PREPROCESSOR_H
#define C99_PREPROCESSOR_H

#include <stdbool.h>
#include "util/arena.h"
#include "util/strtab.h"
#include "util/error.h"
#include "util/vector.h"

/* Macro parameter */
typedef struct MacroParam {
    const char *name;
    struct MacroParam *next;
} MacroParam;

/* Macro definition */
typedef struct Macro {
    const char *name;
    const char *body;          /* replacement text */
    MacroParam *params;        /* NULL for object-like macros */
    int param_count;
    bool is_function_like;
    bool is_builtin;           /* __LINE__, __FILE__, etc. */
    struct Macro *next;        /* hash chain */
} Macro;

#define PP_MACRO_BUCKETS 512

typedef struct Preprocessor {
    Arena arena;
    Strtab *strtab;
    ErrorCtx *err;
    Macro *macros[PP_MACRO_BUCKETS];
    Vector include_paths;      /* const char* */
    const char *filename;
    int line;
} Preprocessor;

void  pp_init(Preprocessor *pp, Strtab *strtab, ErrorCtx *err);
void  pp_free(Preprocessor *pp);
void  pp_add_include_path(Preprocessor *pp, const char *path);
void  pp_define(Preprocessor *pp, const char *name, const char *body);
void  pp_undef(Preprocessor *pp, const char *name);
Macro *pp_lookup(Preprocessor *pp, const char *name);

/* Preprocess a source buffer, returns newly allocated preprocessed text */
char *pp_process(Preprocessor *pp, const char *src, size_t len,
                 const char *filename);

#endif /* C99_PREPROCESSOR_H */
