/*
 * c99lang - Simple register allocator
 */
#include "codegen/regalloc.h"
#include <stdlib.h>
#include <string.h>

const X86Reg param_regs[6] = {
    REG_RDI, REG_RSI, REG_RDX, REG_RCX, REG_R8, REG_R9
};

const X86Reg caller_saved[] = {
    REG_RAX, REG_RCX, REG_RDX, REG_RSI, REG_RDI,
    REG_R8, REG_R9, REG_R10, REG_R11
};

const X86Reg callee_saved[] = {
    REG_RBX, REG_R12, REG_R13, REG_R14, REG_R15
};

/* Register names for different sizes */
static const char *reg_names_64[] = {
    "rax", "rbx", "rcx", "rdx", "rsi", "rdi",
    "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
    "rsp", "rbp"
};
static const char *reg_names_32[] = {
    "eax", "ebx", "ecx", "edx", "esi", "edi",
    "r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d",
    "esp", "ebp"
};
static const char *reg_names_16[] = {
    "ax", "bx", "cx", "dx", "si", "di",
    "r8w", "r9w", "r10w", "r11w", "r12w", "r13w", "r14w", "r15w",
    "sp", "bp"
};
static const char *reg_names_8[] = {
    "al", "bl", "cl", "dl", "sil", "dil",
    "r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b",
    "spl", "bpl"
};

const char *reg_name(X86Reg reg, RegSize size) {
    if (reg < 0 || reg >= REG_COUNT) return "???";
    switch (size) {
    case REG_8:  return reg_names_8[reg];
    case REG_16: return reg_names_16[reg];
    case REG_32: return reg_names_32[reg];
    case REG_64: return reg_names_64[reg];
    }
    return "???";
}

void regalloc_init(RegAlloc *ra, Arena *arena, int num_temps) {
    ra->arena = arena;
    ra->num_temps = num_temps;
    ra->spill_offset = 0;
    ra->temp_to_reg = (int *)arena_alloc(arena, num_temps * sizeof(int));
    ra->temp_to_spill = (int *)arena_alloc(arena, num_temps * sizeof(int));
    memset(ra->temp_to_reg, -1, num_temps * sizeof(int));
    memset(ra->temp_to_spill, 0, num_temps * sizeof(int));
}

void regalloc_run(RegAlloc *ra, IRFunc *func) {
    /* Simple allocation strategy:
     * - First 6 parameters go to param registers
     * - Other temps get caller-saved registers
     * - Spill to stack if no registers available
     */

    /* Available general purpose registers (excluding rsp, rbp, and param regs) */
    X86Reg avail[] = { REG_RAX, REG_RBX, REG_R10, REG_R11, REG_R12,
                       REG_R13, REG_R14, REG_R15 };
    int navail = sizeof(avail) / sizeof(avail[0]);
    int next_reg = 0;

    for (int i = 0; i < ra->num_temps; i++) {
        if (next_reg < navail) {
            ra->temp_to_reg[i] = avail[next_reg++];
        } else {
            ra->temp_to_reg[i] = REG_SPILL;
            ra->spill_offset += 8;
            ra->temp_to_spill[i] = -ra->spill_offset;
        }
    }
}
