/*
 * c99lang - A C99 Compiler
 * x86-64 code generator
 */
#ifndef C99_CODEGEN_X86_64_H
#define C99_CODEGEN_X86_64_H

#include <stdio.h>
#include "ir/ir.h"
#include "util/arena.h"
#include "util/error.h"

typedef struct CodegenX86 {
    Arena *arena;
    FILE *out;               /* output assembly file */
    ErrorCtx *err;
    IRModule *module;
    int stack_offset;
    int *temp_offsets;       /* stack offset for each temp */
    int temp_offset_cap;
} CodegenX86;

void codegen_x86_init(CodegenX86 *cg, Arena *arena, FILE *out, ErrorCtx *err);
void codegen_x86_emit(CodegenX86 *cg, IRModule *module);

#endif /* C99_CODEGEN_X86_64_H */
