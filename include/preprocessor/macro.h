/*
 * c99lang - A C99 Compiler
 * Macro expansion helpers
 */
#ifndef C99_MACRO_H
#define C99_MACRO_H

#include "preprocessor/preprocessor.h"

/* Expand a macro invocation, appending result to 'out' buffer */
void macro_expand(Preprocessor *pp, Macro *m, const char *args[],
                  int nargs, char **out, size_t *out_len, size_t *out_cap);

/* Built-in macros (__LINE__, __FILE__, __DATE__, __TIME__, etc.) */
void macro_init_builtins(Preprocessor *pp);

#endif /* C99_MACRO_H */
