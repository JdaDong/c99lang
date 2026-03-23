/*
 * c99lang - IR infrastructure tests
 */
#include <stdio.h>
#include <string.h>
#include "ir/ir.h"
#include "sema/type.h"
#include "util/arena.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_TRUE(cond, msg) do { \
    if (cond) { tests_passed++; } \
    else { tests_failed++; \
           printf("FAIL: %s at %s:%d\n", msg, __FILE__, __LINE__); } \
} while(0)

#define ASSERT_EQ(a, b, msg) do { \
    if ((long)(a) == (long)(b)) { tests_passed++; } \
    else { tests_failed++; \
           printf("FAIL: %s (expected %ld, got %ld) at %s:%d\n", \
                  msg, (long)(b), (long)(a), __FILE__, __LINE__); } \
} while(0)

static void test_ir_module_new(void) {
    Arena arena;
    arena_init(&arena);

    IRModule *mod = ir_module_new(&arena);
    ASSERT_TRUE(mod != NULL, "ir_module_new non-null");
    ASSERT_TRUE(mod->arena == &arena, "module arena set");
    ASSERT_EQ(mod->functions.size, 0, "no functions initially");
    ASSERT_EQ(mod->globals.size, 0, "no globals initially");
    ASSERT_EQ(mod->string_lits.size, 0, "no string lits initially");

    arena_free(&arena);
}

static void test_ir_func_new(void) {
    Arena arena;
    arena_init(&arena);

    CType *ft = type_func(&arena, type_int_get(), NULL, 0, false);
    IRFunc *func = ir_func_new(&arena, "main", ft);
    ASSERT_TRUE(func != NULL, "ir_func_new non-null");
    ASSERT_TRUE(strcmp(func->name, "main") == 0, "func name is main");
    ASSERT_EQ(func->temp_count, 0, "initial temp_count is 0");
    ASSERT_EQ(func->label_count, 0, "initial label_count is 0");
    ASSERT_EQ(func->blocks.size, 0, "no blocks initially");

    arena_free(&arena);
}

static void test_ir_block_new(void) {
    Arena arena;
    arena_init(&arena);

    IRBlock *b = ir_block_new(&arena, 0);
    ASSERT_TRUE(b != NULL, "ir_block_new non-null");
    ASSERT_EQ(b->label_id, 0, "block label_id is 0");
    ASSERT_EQ(b->instrs.size, 0, "no instrs initially");
    ASSERT_EQ(b->preds.size, 0, "no preds initially");
    ASSERT_EQ(b->succs.size, 0, "no succs initially");

    arena_free(&arena);
}

static void test_ir_instr_new(void) {
    Arena arena;
    arena_init(&arena);

    IRInstr *i = ir_instr_new(&arena, IR_ADD);
    ASSERT_TRUE(i != NULL, "ir_instr_new non-null");
    ASSERT_EQ(i->op, IR_ADD, "instr op is ADD");
    ASSERT_EQ(i->dst.kind, IRV_NONE, "dst is NONE");
    ASSERT_EQ(i->src1.kind, IRV_NONE, "src1 is NONE");
    ASSERT_EQ(i->src2.kind, IRV_NONE, "src2 is NONE");
    ASSERT_EQ(i->label_id, -1, "label_id is -1");
    ASSERT_TRUE(i->comment == NULL, "comment is NULL");

    arena_free(&arena);
}

static void test_ir_val_none(void) {
    IRVal v = ir_val_none();
    ASSERT_EQ(v.kind, IRV_NONE, "val_none kind is NONE");
    ASSERT_TRUE(v.type == NULL, "val_none type is NULL");
}

static void test_ir_val_temp(void) {
    IRVal v = ir_val_temp(42, type_int_get());
    ASSERT_EQ(v.kind, IRV_TEMP, "val_temp kind is TEMP");
    ASSERT_EQ(v.temp_id, 42, "val_temp id is 42");
    ASSERT_TRUE(v.type == type_int_get(), "val_temp type is int");
}

static void test_ir_val_imm_int(void) {
    IRVal v = ir_val_imm_int(100, type_int_get());
    ASSERT_EQ(v.kind, IRV_IMM_INT, "val_imm_int kind");
    ASSERT_EQ(v.imm_int, 100, "val_imm_int value is 100");

    IRVal v2 = ir_val_imm_int(-1, type_int_get());
    ASSERT_EQ(v2.imm_int, -1, "val_imm_int value is -1");

    IRVal v3 = ir_val_imm_int(0, type_int_get());
    ASSERT_EQ(v3.imm_int, 0, "val_imm_int value is 0");
}

static void test_ir_val_imm_float(void) {
    IRVal v = ir_val_imm_float(3.14, type_double_get());
    ASSERT_EQ(v.kind, IRV_IMM_FLOAT, "val_imm_float kind");
    ASSERT_TRUE(v.imm_float > 3.13 && v.imm_float < 3.15, "val_imm_float ~3.14");
}

static void test_ir_val_label(void) {
    IRVal v = ir_val_label(7);
    ASSERT_EQ(v.kind, IRV_LABEL, "val_label kind");
    ASSERT_EQ(v.label_id, 7, "val_label id is 7");
}

static void test_ir_val_global(void) {
    IRVal v = ir_val_global("my_global", type_int_get());
    ASSERT_EQ(v.kind, IRV_GLOBAL, "val_global kind");
    ASSERT_TRUE(strcmp(v.name, "my_global") == 0, "val_global name");
}

static void test_ir_build_function(void) {
    Arena arena;
    arena_init(&arena);

    IRModule *mod = ir_module_new(&arena);
    CType *ft = type_func(&arena, type_int_get(), NULL, 0, false);
    IRFunc *func = ir_func_new(&arena, "test_func", ft);

    /* Create entry block */
    IRBlock *entry = ir_block_new(&arena, 0);

    /* Build: t0 = load_imm 42 */
    IRInstr *load = ir_instr_new(&arena, IR_LOAD_IMM);
    load->dst = ir_val_temp(0, type_int_get());
    load->src1 = ir_val_imm_int(42, type_int_get());
    vec_push(&entry->instrs, load);

    /* Build: ret t0 */
    IRInstr *ret = ir_instr_new(&arena, IR_RET);
    ret->src1 = ir_val_temp(0, type_int_get());
    vec_push(&entry->instrs, ret);

    vec_push(&func->blocks, entry);
    func->temp_count = 1;
    func->label_count = 1;

    vec_push(&mod->functions, func);

    ASSERT_EQ(mod->functions.size, 1, "module has 1 function");
    IRFunc *f = (IRFunc *)vec_get(&mod->functions, 0);
    ASSERT_TRUE(strcmp(f->name, "test_func") == 0, "func name");
    ASSERT_EQ(f->blocks.size, 1, "func has 1 block");

    IRBlock *b = (IRBlock *)vec_get(&f->blocks, 0);
    ASSERT_EQ(b->instrs.size, 2, "block has 2 instrs");

    IRInstr *i0 = (IRInstr *)vec_get(&b->instrs, 0);
    ASSERT_EQ(i0->op, IR_LOAD_IMM, "first instr is load_imm");
    ASSERT_EQ(i0->dst.temp_id, 0, "dst is t0");

    IRInstr *i1 = (IRInstr *)vec_get(&b->instrs, 1);
    ASSERT_EQ(i1->op, IR_RET, "second instr is ret");

    arena_free(&arena);
}

static void test_ir_op_str(void) {
    ASSERT_TRUE(strcmp(ir_op_str(IR_ADD), "add") == 0, "IR_ADD str");
    ASSERT_TRUE(strcmp(ir_op_str(IR_SUB), "sub") == 0, "IR_SUB str");
    ASSERT_TRUE(strcmp(ir_op_str(IR_MUL), "mul") == 0, "IR_MUL str");
    ASSERT_TRUE(strcmp(ir_op_str(IR_DIV), "div") == 0, "IR_DIV str");
    ASSERT_TRUE(strcmp(ir_op_str(IR_MOD), "mod") == 0, "IR_MOD str");
    ASSERT_TRUE(strcmp(ir_op_str(IR_RET), "ret") == 0, "IR_RET str");
    ASSERT_TRUE(strcmp(ir_op_str(IR_JMP), "jmp") == 0, "IR_JMP str");
    ASSERT_TRUE(strcmp(ir_op_str(IR_JZ), "jz") == 0, "IR_JZ str");
    ASSERT_TRUE(strcmp(ir_op_str(IR_JNZ), "jnz") == 0, "IR_JNZ str");
    ASSERT_TRUE(strcmp(ir_op_str(IR_CALL), "call") == 0, "IR_CALL str");
    ASSERT_TRUE(strcmp(ir_op_str(IR_ALLOCA), "alloca") == 0, "IR_ALLOCA str");
    ASSERT_TRUE(strcmp(ir_op_str(IR_NOP), "nop") == 0, "IR_NOP str");
    ASSERT_TRUE(strcmp(ir_op_str(IR_EQ), "eq") == 0, "IR_EQ str");
    ASSERT_TRUE(strcmp(ir_op_str(IR_NE), "ne") == 0, "IR_NE str");
    ASSERT_TRUE(strcmp(ir_op_str(IR_LT), "lt") == 0, "IR_LT str");
}

static void test_ir_all_opcodes(void) {
    /* Just ensure ir_instr_new works for all opcodes */
    Arena arena;
    arena_init(&arena);

    IROp ops[] = {
        IR_ADD, IR_SUB, IR_MUL, IR_DIV, IR_MOD, IR_NEG,
        IR_AND, IR_OR, IR_XOR, IR_NOT, IR_SHL, IR_SHR,
        IR_EQ, IR_NE, IR_LT, IR_LE, IR_GT, IR_GE,
        IR_LOAD, IR_STORE, IR_MOV, IR_LOAD_IMM, IR_LEA,
        IR_JMP, IR_JZ, IR_JNZ, IR_LABEL,
        IR_CALL, IR_ARG, IR_RET, IR_FUNC_BEGIN, IR_FUNC_END,
        IR_TRUNC, IR_ZEXT, IR_SEXT, IR_FPTOSI, IR_SITOFP,
        IR_FPEXT, IR_FPTRUNC, IR_PTRTOINT, IR_INTTOPTR, IR_BITCAST,
        IR_ALLOCA, IR_NOP
    };
    int n = sizeof(ops) / sizeof(ops[0]);

    for (int i = 0; i < n; i++) {
        IRInstr *instr = ir_instr_new(&arena, ops[i]);
        ASSERT_TRUE(instr != NULL, "ir_instr_new for all opcodes");
        ASSERT_EQ(instr->op, ops[i], "opcode matches");
    }

    arena_free(&arena);
}

int main(void) {
    printf("=== IR Tests ===\n");

    test_ir_module_new();
    test_ir_func_new();
    test_ir_block_new();
    test_ir_instr_new();
    test_ir_val_none();
    test_ir_val_temp();
    test_ir_val_imm_int();
    test_ir_val_imm_float();
    test_ir_val_label();
    test_ir_val_global();
    test_ir_build_function();
    test_ir_op_str();
    test_ir_all_opcodes();

    printf("\n%d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
