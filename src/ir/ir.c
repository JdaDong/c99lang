/*
 * c99lang - IR infrastructure
 */
#include "ir/ir.h"
#include <stdio.h>
#include <string.h>

IRModule *ir_module_new(Arena *a) {
    IRModule *m = (IRModule *)arena_calloc(a, sizeof(IRModule));
    m->arena = a;
    vec_init(&m->functions);
    vec_init(&m->globals);
    vec_init(&m->string_lits);
    return m;
}

IRFunc *ir_func_new(Arena *a, const char *name, CType *type) {
    IRFunc *f = (IRFunc *)arena_calloc(a, sizeof(IRFunc));
    f->name = name;
    f->type = type;
    vec_init(&f->blocks);
    vec_init(&f->params);
    f->temp_count = 0;
    f->label_count = 0;
    f->stack_size = 0;
    return f;
}

IRBlock *ir_block_new(Arena *a, int label_id) {
    IRBlock *b = (IRBlock *)arena_calloc(a, sizeof(IRBlock));
    b->label_id = label_id;
    vec_init(&b->instrs);
    vec_init(&b->preds);
    vec_init(&b->succs);
    return b;
}

IRInstr *ir_instr_new(Arena *a, IROp op) {
    IRInstr *i = (IRInstr *)arena_calloc(a, sizeof(IRInstr));
    i->op = op;
    i->dst = ir_val_none();
    i->src1 = ir_val_none();
    i->src2 = ir_val_none();
    i->label_id = -1;
    i->comment = NULL;
    return i;
}

/* Value constructors */

IRVal ir_val_none(void) {
    IRVal v = { .kind = IRV_NONE, .type = NULL };
    return v;
}

IRVal ir_val_temp(int id, CType *type) {
    IRVal v = { .kind = IRV_TEMP, .type = type, .temp_id = id };
    return v;
}

IRVal ir_val_imm_int(long long val, CType *type) {
    IRVal v = { .kind = IRV_IMM_INT, .type = type, .imm_int = val };
    return v;
}

IRVal ir_val_imm_float(double val, CType *type) {
    IRVal v = { .kind = IRV_IMM_FLOAT, .type = type, .imm_float = val };
    return v;
}

IRVal ir_val_label(int id) {
    IRVal v = { .kind = IRV_LABEL, .type = NULL, .label_id = id };
    return v;
}

IRVal ir_val_global(const char *name, CType *type) {
    IRVal v = { .kind = IRV_GLOBAL, .type = type, .name = name };
    return v;
}

/* Pretty printer */

const char *ir_op_str(IROp op) {
    switch (op) {
    case IR_ADD: return "add"; case IR_SUB: return "sub";
    case IR_MUL: return "mul"; case IR_DIV: return "div";
    case IR_MOD: return "mod"; case IR_NEG: return "neg";
    case IR_AND: return "and"; case IR_OR:  return "or";
    case IR_XOR: return "xor"; case IR_NOT: return "not";
    case IR_SHL: return "shl"; case IR_SHR: return "shr";
    case IR_EQ:  return "eq";  case IR_NE:  return "ne";
    case IR_LT:  return "lt";  case IR_LE:  return "le";
    case IR_GT:  return "gt";  case IR_GE:  return "ge";
    case IR_LOAD: return "load"; case IR_STORE: return "store";
    case IR_MOV: return "mov"; case IR_LOAD_IMM: return "load_imm";
    case IR_LEA: return "lea";
    case IR_JMP: return "jmp"; case IR_JZ: return "jz";
    case IR_JNZ: return "jnz"; case IR_LABEL: return "label";
    case IR_CALL: return "call"; case IR_ARG: return "arg";
    case IR_RET: return "ret";
    case IR_FUNC_BEGIN: return "func_begin";
    case IR_FUNC_END: return "func_end";
    case IR_TRUNC: return "trunc"; case IR_ZEXT: return "zext";
    case IR_SEXT: return "sext"; case IR_FPTOSI: return "fptosi";
    case IR_SITOFP: return "sitofp"; case IR_FPEXT: return "fpext";
    case IR_FPTRUNC: return "fptrunc";
    case IR_PTRTOINT: return "ptrtoint"; case IR_INTTOPTR: return "inttoptr";
    case IR_BITCAST: return "bitcast";
    case IR_ALLOCA: return "alloca";
    case IR_NOP: return "nop";
    }
    return "???";
}

static void print_val(const IRVal *v) {
    switch (v->kind) {
    case IRV_NONE: printf("_"); break;
    case IRV_TEMP: printf("t%d", v->temp_id); break;
    case IRV_IMM_INT: printf("%lld", v->imm_int); break;
    case IRV_IMM_FLOAT: printf("%g", v->imm_float); break;
    case IRV_LABEL: printf("L%d", v->label_id); break;
    case IRV_GLOBAL: printf("@%s", v->name); break;
    case IRV_STRING: printf("\"%s\"", v->name); break;
    }
}

void ir_print_instr(const IRInstr *i) {
    printf("    ");

    switch (i->op) {
    case IR_LABEL:
        printf("L%d:", i->label_id);
        break;
    case IR_JMP:
        printf("jmp L%d", i->label_id);
        break;
    case IR_JZ:
        printf("jz "); print_val(&i->src1); printf(", L%d", i->label_id);
        break;
    case IR_JNZ:
        printf("jnz "); print_val(&i->src1); printf(", L%d", i->label_id);
        break;
    case IR_RET:
        printf("ret "); print_val(&i->src1);
        break;
    case IR_CALL:
        print_val(&i->dst); printf(" = call ");
        print_val(&i->src1);
        break;
    case IR_ARG:
        printf("arg "); print_val(&i->src1);
        break;
    case IR_FUNC_BEGIN:
    case IR_FUNC_END:
        printf("%s", ir_op_str(i->op));
        break;
    default:
        print_val(&i->dst); printf(" = %s ", ir_op_str(i->op));
        print_val(&i->src1);
        if (i->src2.kind != IRV_NONE) {
            printf(", "); print_val(&i->src2);
        }
        break;
    }

    if (i->comment) printf("  ; %s", i->comment);
    printf("\n");
}

void ir_print_func(const IRFunc *func) {
    printf("\nfunc %s (%d temps, %d labels):\n",
           func->name, func->temp_count, func->label_count);
    for (size_t bi = 0; bi < func->blocks.size; bi++) {
        IRBlock *block = (IRBlock *)vec_get(&func->blocks, bi);
        for (size_t ii = 0; ii < block->instrs.size; ii++) {
            ir_print_instr((IRInstr *)vec_get(&block->instrs, ii));
        }
    }
}

void ir_print_module(const IRModule *mod) {
    printf("=== IR Module ===\n");
    for (size_t i = 0; i < mod->functions.size; i++) {
        ir_print_func((IRFunc *)vec_get(&mod->functions, i));
    }
    printf("=================\n");
}
