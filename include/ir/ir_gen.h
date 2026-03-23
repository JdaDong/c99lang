/*
 * c99lang - A C99 Compiler
 * IR generation from AST
 */
#ifndef C99_IR_GEN_H
#define C99_IR_GEN_H

#include "ir/ir.h"
#include "parser/ast.h"
#include "sema/symtab.h"
#include "util/arena.h"
#include "util/error.h"

typedef struct IRGen {
    Arena *arena;
    IRModule *module;
    IRFunc *current_func;
    IRBlock *current_block;
    SymTab *symtab;
    ErrorCtx *err;
    int temp_counter;
    int label_counter;

    /* Break / continue targets */
    int break_label;
    int continue_label;
} IRGen;

void      ir_gen_init(IRGen *g, Arena *arena, SymTab *symtab, ErrorCtx *err);
IRModule *ir_gen_translate(IRGen *g, ASTNode *tu);

#endif /* C99_IR_GEN_H */
