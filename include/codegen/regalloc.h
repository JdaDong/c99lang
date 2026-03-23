/*
 * c99lang - A C99 Compiler
 * Simple register allocator (linear scan)
 */
#ifndef C99_REGALLOC_H
#define C99_REGALLOC_H

#include "ir/ir.h"
#include "util/arena.h"

/* x86-64 general purpose registers */
typedef enum X86Reg {
    REG_RAX = 0, REG_RBX, REG_RCX, REG_RDX,
    REG_RSI, REG_RDI, REG_R8,  REG_R9,
    REG_R10, REG_R11, REG_R12, REG_R13,
    REG_R14, REG_R15,
    REG_RSP, REG_RBP,
    REG_COUNT,
    REG_NONE = -1,
    REG_SPILL = -2,      /* spilled to stack */
} X86Reg;

/* Register size suffix */
typedef enum RegSize {
    REG_8  = 1,
    REG_16 = 2,
    REG_32 = 4,
    REG_64 = 8,
} RegSize;

const char *reg_name(X86Reg reg, RegSize size);

/* Calling convention: parameter registers */
extern const X86Reg param_regs[6];       /* rdi, rsi, rdx, rcx, r8, r9 */
extern const X86Reg caller_saved[];
extern const X86Reg callee_saved[];

/* Simple allocation map: temp_id -> register or stack offset */
typedef struct RegAlloc {
    Arena *arena;
    int *temp_to_reg;        /* temp_id -> X86Reg or REG_SPILL */
    int *temp_to_spill;      /* spill offset (if REG_SPILL) */
    int num_temps;
    int spill_offset;        /* current spill stack offset */
} RegAlloc;

void regalloc_init(RegAlloc *ra, Arena *arena, int num_temps);
void regalloc_run(RegAlloc *ra, IRFunc *func);

#endif /* C99_REGALLOC_H */
