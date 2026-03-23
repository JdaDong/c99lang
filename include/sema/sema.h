/*
 * c99lang - A C99 Compiler
 * Semantic analysis
 */
#ifndef C99_SEMA_H
#define C99_SEMA_H

#include "parser/ast.h"
#include "sema/symtab.h"
#include "sema/type.h"
#include "util/arena.h"
#include "util/error.h"

typedef struct Sema {
    Arena *arena;
    SymTab symtab;
    ErrorCtx *err;
    CType *current_func_ret;  /* return type of current function */
    int loop_depth;           /* for break/continue checking */
    int switch_depth;         /* for case/default checking */
} Sema;

void sema_init(Sema *s, Arena *arena, ErrorCtx *err);
void sema_check(Sema *s, ASTNode *tu);

/* Per-node type checking */
CType *sema_check_expr(Sema *s, ASTNode *expr);
void   sema_check_stmt(Sema *s, ASTNode *stmt);
void   sema_check_decl(Sema *s, ASTNode *decl);

/* Implicit conversions */
CType *sema_usual_arithmetic_conversions(CType *a, CType *b);
CType *sema_integer_promotions(CType *t);

#endif /* C99_SEMA_H */
