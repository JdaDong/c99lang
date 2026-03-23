/*
 * c99lang - A C99 Compiler
 * Intermediate Representation (Three-Address Code)
 */
#ifndef C99_IR_H
#define C99_IR_H

#include <stdbool.h>
#include "sema/type.h"
#include "util/arena.h"
#include "util/vector.h"

/* IR opcodes */
typedef enum IROp {
    /* Arithmetic */
    IR_ADD, IR_SUB, IR_MUL, IR_DIV, IR_MOD,
    IR_NEG,

    /* Bitwise */
    IR_AND, IR_OR, IR_XOR, IR_NOT,
    IR_SHL, IR_SHR,

    /* Comparison */
    IR_EQ, IR_NE, IR_LT, IR_LE, IR_GT, IR_GE,

    /* Data movement */
    IR_LOAD,          /* t = *addr */
    IR_STORE,         /* *addr = t */
    IR_MOV,           /* t1 = t2 */
    IR_LOAD_IMM,      /* t = imm */
    IR_LEA,           /* t = &var */

    /* Control flow */
    IR_JMP,           /* goto label */
    IR_JZ,            /* if !t goto label */
    IR_JNZ,           /* if  t goto label */
    IR_LABEL,

    /* Function call */
    IR_CALL,          /* t = call func(args...) */
    IR_ARG,           /* push argument */
    IR_RET,           /* return t */
    IR_FUNC_BEGIN,
    IR_FUNC_END,

    /* Type conversion */
    IR_TRUNC,
    IR_ZEXT,
    IR_SEXT,
    IR_FPTOSI,
    IR_SITOFP,
    IR_FPEXT,
    IR_FPTRUNC,
    IR_PTRTOINT,
    IR_INTTOPTR,
    IR_BITCAST,

    /* Memory */
    IR_ALLOCA,        /* stack allocation */

    IR_NOP,
} IROp;

/* Operand kinds */
typedef enum IRValKind {
    IRV_NONE,
    IRV_TEMP,         /* virtual register / temporary */
    IRV_IMM_INT,      /* integer immediate */
    IRV_IMM_FLOAT,    /* float immediate */
    IRV_LABEL,        /* label reference */
    IRV_GLOBAL,       /* global variable name */
    IRV_STRING,       /* string literal */
} IRValKind;

typedef struct IRVal {
    IRValKind kind;
    CType *type;
    union {
        int temp_id;
        long long imm_int;
        double imm_float;
        int label_id;
        const char *name;
    };
} IRVal;

/* A single IR instruction */
typedef struct IRInstr {
    IROp op;
    IRVal dst;
    IRVal src1;
    IRVal src2;
    int label_id;            /* for IR_LABEL, IR_JMP, IR_JZ, IR_JNZ */
    const char *comment;     /* optional debug comment */
} IRInstr;

/* Basic block */
typedef struct IRBlock {
    int label_id;
    Vector instrs;           /* IRInstr* */
    Vector preds;            /* IRBlock* */
    Vector succs;            /* IRBlock* */
} IRBlock;

/* Function in IR */
typedef struct IRFunc {
    const char *name;
    CType *type;
    Vector blocks;           /* IRBlock* */
    int temp_count;
    int label_count;
    int stack_size;
    Vector params;           /* IRVal */
} IRFunc;

/* IR module (whole translation unit) */
typedef struct IRModule {
    Arena *arena;
    Vector functions;        /* IRFunc* */
    Vector globals;          /* IRVal (global variables) */
    Vector string_lits;      /* const char* */
} IRModule;

/* Construction */
IRModule *ir_module_new(Arena *a);
IRFunc   *ir_func_new(Arena *a, const char *name, CType *type);
IRBlock  *ir_block_new(Arena *a, int label_id);
IRInstr  *ir_instr_new(Arena *a, IROp op);

IRVal ir_val_none(void);
IRVal ir_val_temp(int id, CType *type);
IRVal ir_val_imm_int(long long v, CType *type);
IRVal ir_val_imm_float(double v, CType *type);
IRVal ir_val_label(int id);
IRVal ir_val_global(const char *name, CType *type);

/* Pretty printer */
void ir_print_module(const IRModule *mod);
void ir_print_func(const IRFunc *func);
void ir_print_instr(const IRInstr *instr);
const char *ir_op_str(IROp op);

#endif /* C99_IR_H */
