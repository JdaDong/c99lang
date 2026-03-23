/*
 * c99lang - x86-64 code generator
 * Emits AT&T syntax x86-64 assembly from IR
 */
#include "codegen/codegen_x86_64.h"
#include "codegen/regalloc.h"
#include <string.h>

void codegen_x86_init(CodegenX86 *cg, Arena *arena, FILE *out, ErrorCtx *err) {
    cg->arena = arena;
    cg->out = out;
    cg->err = err;
    cg->module = NULL;
    cg->stack_offset = 0;
    cg->temp_offsets = NULL;
    cg->temp_offset_cap = 0;
}

static void emit_raw(CodegenX86 *cg, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(cg->out, fmt, ap);
    va_end(ap);
}

static void emit_line(CodegenX86 *cg, const char *fmt, ...) {
    fprintf(cg->out, "    ");
    va_list ap;
    va_start(ap, fmt);
    vfprintf(cg->out, fmt, ap);
    va_end(ap);
    fprintf(cg->out, "\n");
}

static void emit_label_line(CodegenX86 *cg, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(cg->out, fmt, ap);
    va_end(ap);
    fprintf(cg->out, "\n");
}

/* Ensure temp has a stack slot */
static int ensure_temp_offset(CodegenX86 *cg, int temp_id) {
    if (temp_id < 0) return 0;

    while (temp_id >= cg->temp_offset_cap) {
        int new_cap = cg->temp_offset_cap == 0 ? 64 : cg->temp_offset_cap * 2;
        int *new_offsets = (int *)arena_alloc(cg->arena, new_cap * sizeof(int));
        if (cg->temp_offsets) {
            memcpy(new_offsets, cg->temp_offsets, cg->temp_offset_cap * sizeof(int));
        }
        for (int i = cg->temp_offset_cap; i < new_cap; i++) new_offsets[i] = 0;
        cg->temp_offsets = new_offsets;
        cg->temp_offset_cap = new_cap;
    }

    if (cg->temp_offsets[temp_id] == 0) {
        cg->stack_offset += 8;
        cg->temp_offsets[temp_id] = -(cg->stack_offset);
    }

    return cg->temp_offsets[temp_id];
}

/* Load an IR value into a register */
static void load_val(CodegenX86 *cg, const IRVal *v, const char *reg) {
    switch (v->kind) {
    case IRV_TEMP: {
        int off = ensure_temp_offset(cg, v->temp_id);
        emit_line(cg, "movq %d(%%rbp), %s", off, reg);
        break;
    }
    case IRV_IMM_INT:
        emit_line(cg, "movq $%lld, %s", v->imm_int, reg);
        break;
    case IRV_GLOBAL:
        emit_line(cg, "leaq %s(%%rip), %s", v->name, reg);
        break;
    case IRV_LABEL:
        emit_line(cg, "leaq .L%d(%%rip), %s", v->label_id, reg);
        break;
    default:
        emit_line(cg, "xorq %s, %s", reg, reg);
        break;
    }
}

/* Store a register to an IR temp */
static void store_temp(CodegenX86 *cg, const char *reg, int temp_id) {
    int off = ensure_temp_offset(cg, temp_id);
    emit_line(cg, "movq %s, %d(%%rbp)", reg, off);
}

/* Generate code for a single IR instruction */
static void gen_instr(CodegenX86 *cg, const IRInstr *instr) {
    switch (instr->op) {
    case IR_FUNC_BEGIN:
        /* Prologue already emitted */
        break;

    case IR_FUNC_END:
        /* Epilogue already emitted */
        break;

    case IR_LABEL:
        emit_label_line(cg, ".L%d:", instr->label_id);
        break;

    case IR_LOAD_IMM:
    case IR_MOV:
        load_val(cg, &instr->src1, "%rax");
        if (instr->dst.kind == IRV_TEMP) {
            store_temp(cg, "%rax", instr->dst.temp_id);
        }
        break;

    case IR_ADD:
        load_val(cg, &instr->src1, "%rax");
        load_val(cg, &instr->src2, "%rcx");
        emit_line(cg, "addq %%rcx, %%rax");
        if (instr->dst.kind == IRV_TEMP)
            store_temp(cg, "%rax", instr->dst.temp_id);
        break;

    case IR_SUB:
        load_val(cg, &instr->src1, "%rax");
        load_val(cg, &instr->src2, "%rcx");
        emit_line(cg, "subq %%rcx, %%rax");
        if (instr->dst.kind == IRV_TEMP)
            store_temp(cg, "%rax", instr->dst.temp_id);
        break;

    case IR_MUL:
        load_val(cg, &instr->src1, "%rax");
        load_val(cg, &instr->src2, "%rcx");
        emit_line(cg, "imulq %%rcx, %%rax");
        if (instr->dst.kind == IRV_TEMP)
            store_temp(cg, "%rax", instr->dst.temp_id);
        break;

    case IR_DIV:
        load_val(cg, &instr->src1, "%rax");
        emit_line(cg, "cqto");
        load_val(cg, &instr->src2, "%rcx");
        emit_line(cg, "idivq %%rcx");
        if (instr->dst.kind == IRV_TEMP)
            store_temp(cg, "%rax", instr->dst.temp_id);
        break;

    case IR_MOD:
        load_val(cg, &instr->src1, "%rax");
        emit_line(cg, "cqto");
        load_val(cg, &instr->src2, "%rcx");
        emit_line(cg, "idivq %%rcx");
        if (instr->dst.kind == IRV_TEMP)
            store_temp(cg, "%rdx", instr->dst.temp_id); /* remainder in rdx */
        break;

    case IR_NEG:
        load_val(cg, &instr->src1, "%rax");
        emit_line(cg, "negq %%rax");
        if (instr->dst.kind == IRV_TEMP)
            store_temp(cg, "%rax", instr->dst.temp_id);
        break;

    case IR_NOT:
        load_val(cg, &instr->src1, "%rax");
        emit_line(cg, "notq %%rax");
        if (instr->dst.kind == IRV_TEMP)
            store_temp(cg, "%rax", instr->dst.temp_id);
        break;

    case IR_AND:
        load_val(cg, &instr->src1, "%rax");
        load_val(cg, &instr->src2, "%rcx");
        emit_line(cg, "andq %%rcx, %%rax");
        if (instr->dst.kind == IRV_TEMP)
            store_temp(cg, "%rax", instr->dst.temp_id);
        break;

    case IR_OR:
        load_val(cg, &instr->src1, "%rax");
        load_val(cg, &instr->src2, "%rcx");
        emit_line(cg, "orq %%rcx, %%rax");
        if (instr->dst.kind == IRV_TEMP)
            store_temp(cg, "%rax", instr->dst.temp_id);
        break;

    case IR_XOR:
        load_val(cg, &instr->src1, "%rax");
        load_val(cg, &instr->src2, "%rcx");
        emit_line(cg, "xorq %%rcx, %%rax");
        if (instr->dst.kind == IRV_TEMP)
            store_temp(cg, "%rax", instr->dst.temp_id);
        break;

    case IR_SHL:
        load_val(cg, &instr->src1, "%rax");
        load_val(cg, &instr->src2, "%rcx");
        emit_line(cg, "shlq %%cl, %%rax");
        if (instr->dst.kind == IRV_TEMP)
            store_temp(cg, "%rax", instr->dst.temp_id);
        break;

    case IR_SHR:
        load_val(cg, &instr->src1, "%rax");
        load_val(cg, &instr->src2, "%rcx");
        emit_line(cg, "sarq %%cl, %%rax");
        if (instr->dst.kind == IRV_TEMP)
            store_temp(cg, "%rax", instr->dst.temp_id);
        break;

    case IR_EQ: case IR_NE: case IR_LT: case IR_LE:
    case IR_GT: case IR_GE: {
        load_val(cg, &instr->src1, "%rax");
        load_val(cg, &instr->src2, "%rcx");
        emit_line(cg, "cmpq %%rcx, %%rax");
        const char *setcc;
        switch (instr->op) {
        case IR_EQ: setcc = "sete"; break;
        case IR_NE: setcc = "setne"; break;
        case IR_LT: setcc = "setl"; break;
        case IR_LE: setcc = "setle"; break;
        case IR_GT: setcc = "setg"; break;
        case IR_GE: setcc = "setge"; break;
        default: setcc = "sete"; break;
        }
        emit_line(cg, "%s %%al", setcc);
        emit_line(cg, "movzbq %%al, %%rax");
        if (instr->dst.kind == IRV_TEMP)
            store_temp(cg, "%rax", instr->dst.temp_id);
        break;
    }

    case IR_JMP:
        emit_line(cg, "jmp .L%d", instr->label_id);
        break;

    case IR_JZ:
        load_val(cg, &instr->src1, "%rax");
        emit_line(cg, "testq %%rax, %%rax");
        emit_line(cg, "jz .L%d", instr->label_id);
        break;

    case IR_JNZ:
        load_val(cg, &instr->src1, "%rax");
        emit_line(cg, "testq %%rax, %%rax");
        emit_line(cg, "jnz .L%d", instr->label_id);
        break;

    case IR_CALL: {
        /* Load function address */
        if (instr->src1.kind == IRV_GLOBAL) {
            emit_line(cg, "callq %s", instr->src1.name);
        } else {
            load_val(cg, &instr->src1, "%rax");
            emit_line(cg, "callq *%%rax");
        }
        /* Result in rax */
        if (instr->dst.kind == IRV_TEMP)
            store_temp(cg, "%rax", instr->dst.temp_id);
        break;
    }

    case IR_ARG: {
        /* Arguments are pushed before call.
         * For System V ABI: first 6 in rdi,rsi,rdx,rcx,r8,r9 */
        /* Simplified: just push to stack for now */
        load_val(cg, &instr->src1, "%rax");
        emit_line(cg, "pushq %%rax");
        break;
    }

    case IR_RET:
        if (instr->src1.kind != IRV_NONE) {
            load_val(cg, &instr->src1, "%rax");
        }
        emit_line(cg, "jmp .Lreturn");
        break;

    case IR_ALLOCA:
        if (instr->dst.kind == IRV_TEMP) {
            ensure_temp_offset(cg, instr->dst.temp_id);
        }
        break;

    case IR_LOAD:
        load_val(cg, &instr->src1, "%rax");
        emit_line(cg, "movq (%%rax), %%rax");
        if (instr->dst.kind == IRV_TEMP)
            store_temp(cg, "%rax", instr->dst.temp_id);
        break;

    case IR_STORE:
        load_val(cg, &instr->src1, "%rax");
        load_val(cg, &instr->dst, "%rcx");
        emit_line(cg, "movq %%rax, (%%rcx)");
        break;

    case IR_LEA:
        if (instr->src1.kind == IRV_TEMP) {
            int off = ensure_temp_offset(cg, instr->src1.temp_id);
            emit_line(cg, "leaq %d(%%rbp), %%rax", off);
        } else {
            load_val(cg, &instr->src1, "%rax");
        }
        if (instr->dst.kind == IRV_TEMP)
            store_temp(cg, "%rax", instr->dst.temp_id);
        break;

    case IR_BITCAST:
    case IR_ZEXT:
    case IR_SEXT:
    case IR_TRUNC:
        load_val(cg, &instr->src1, "%rax");
        if (instr->dst.kind == IRV_TEMP)
            store_temp(cg, "%rax", instr->dst.temp_id);
        break;

    case IR_NOP:
        emit_line(cg, "nop");
        break;

    default:
        emit_line(cg, "# unhandled IR op: %s", ir_op_str(instr->op));
        break;
    }
}

/* Generate code for a function */
static void gen_func(CodegenX86 *cg, const IRFunc *func) {
    cg->stack_offset = 0;
    cg->temp_offsets = NULL;
    cg->temp_offset_cap = 0;

    /* Pre-allocate stack space for all temps */
    int total_stack = (func->temp_count + 2) * 8;
    /* Align to 16 bytes */
    total_stack = (total_stack + 15) & ~15;

    /* Emit function header */
    emit_raw(cg, "\n");
    emit_raw(cg, "    .globl %s\n", func->name);
    emit_raw(cg, "    .type %s, @function\n", func->name);
    emit_label_line(cg, "%s:", func->name);

    /* Prologue */
    emit_line(cg, "pushq %%rbp");
    emit_line(cg, "movq %%rsp, %%rbp");
    emit_line(cg, "subq $%d, %%rsp", total_stack);

    /* Save callee-saved registers if needed */
    /* (simplified: we save rbx always) */
    emit_line(cg, "pushq %%rbx");

    /* Copy parameters from registers to stack slots */
    const char *param_reg_names[] = { "%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9" };
    int nparam = func->params.size < 6 ? (int)func->params.size : 6;
    for (int i = 0; i < nparam; i++) {
        int off = ensure_temp_offset(cg, i);
        emit_line(cg, "movq %s, %d(%%rbp)", param_reg_names[i], off);
    }

    /* Generate code for all blocks */
    for (size_t bi = 0; bi < func->blocks.size; bi++) {
        IRBlock *block = (IRBlock *)vec_get(&func->blocks, bi);
        for (size_t ii = 0; ii < block->instrs.size; ii++) {
            IRInstr *instr = (IRInstr *)vec_get(&block->instrs, ii);
            if (instr->op == IR_FUNC_BEGIN || instr->op == IR_FUNC_END)
                continue;
            gen_instr(cg, instr);
        }
    }

    /* Epilogue */
    emit_label_line(cg, ".Lreturn:");
    emit_line(cg, "popq %%rbx");
    emit_line(cg, "movq %%rbp, %%rsp");
    emit_line(cg, "popq %%rbp");
    emit_line(cg, "retq");

    emit_raw(cg, "    .size %s, .-%s\n", func->name, func->name);
}

/* Generate the full assembly output */
void codegen_x86_emit(CodegenX86 *cg, IRModule *module) {
    cg->module = module;

    /* Assembly header */
    emit_raw(cg, "# Generated by c99c - C99 Compiler\n");
    emit_raw(cg, "    .text\n");

    /* String literals */
    if (module->string_lits.size > 0) {
        emit_raw(cg, "    .section .rodata\n");
        for (size_t i = 0; i < module->string_lits.size; i++) {
            const char *str = (const char *)vec_get(&module->string_lits, i);
            emit_raw(cg, ".str.%zu:\n", i);
            emit_raw(cg, "    .string %s\n", str);
        }
        emit_raw(cg, "    .text\n");
    }

    /* Functions */
    for (size_t i = 0; i < module->functions.size; i++) {
        IRFunc *func = (IRFunc *)vec_get(&module->functions, i);
        gen_func(cg, func);
    }

    /* Section note for GNU stack (no executable stack) */
    emit_raw(cg, "\n    .section .note.GNU-stack,\"\",@progbits\n");
}
