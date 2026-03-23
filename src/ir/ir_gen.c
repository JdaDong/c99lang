/*
 * c99lang - IR generation from AST
 * Translates the typed AST into three-address code IR
 */
#include "ir/ir_gen.h"
#include <string.h>
#include <stdio.h>

void ir_gen_init(IRGen *g, Arena *arena, SymTab *symtab, ErrorCtx *err) {
    g->arena = arena;
    g->module = ir_module_new(arena);
    g->current_func = NULL;
    g->current_block = NULL;
    g->symtab = symtab;
    g->err = err;
    g->temp_counter = 0;
    g->label_counter = 0;
    g->break_label = -1;
    g->continue_label = -1;
}

static int new_temp(IRGen *g) {
    return g->temp_counter++;
}

static int new_label(IRGen *g) {
    return g->label_counter++;
}

static void emit(IRGen *g, IRInstr *instr) {
    if (g->current_block) {
        vec_push(&g->current_block->instrs, instr);
    }
}

static void emit_label(IRGen *g, int label_id) {
    IRInstr *i = ir_instr_new(g->arena, IR_LABEL);
    i->label_id = label_id;
    emit(g, i);
}

static IRVal gen_expr(IRGen *g, ASTNode *expr);
static void gen_stmt(IRGen *g, ASTNode *stmt);
static void gen_decl(IRGen *g, ASTNode *decl);

/* ---- Expression IR generation ---- */

static IRVal gen_expr(IRGen *g, ASTNode *expr) {
    if (!expr) return ir_val_none();

    switch (expr->kind) {
    case AST_INT_LIT:
        return ir_val_imm_int((long long)expr->int_lit.value,
                              expr->type ? expr->type : type_int_get());

    case AST_FLOAT_LIT:
        return ir_val_imm_float((double)expr->float_lit.value,
                                expr->type ? expr->type : type_double_get());

    case AST_CHAR_LIT:
        return ir_val_imm_int(expr->char_lit.value, type_int_get());

    case AST_STRING_LIT: {
        /* Store string in module's string table */
        int idx = (int)g->module->string_lits.size;
        vec_push(&g->module->string_lits, (void *)expr->string_lit.value);
        char name[32];
        snprintf(name, sizeof(name), ".str.%d", idx);
        const char *sname = arena_strdup(g->arena, name);
        return ir_val_global(sname, expr->type);
    }

    case AST_IDENT_EXPR: {
        Symbol *sym = symtab_lookup(g->symtab, expr->ident.name);
        if (sym) {
            if (sym->ir_slot >= 0) {
                return ir_val_temp(sym->ir_slot,
                                   expr->type ? expr->type : type_int_get());
            }
            /* Global or function */
            if (sym->kind == SYM_FUNC || sym->linkage == LINK_EXTERNAL) {
                return ir_val_global(sym->name, sym->type);
            }
        }
        return ir_val_global(expr->ident.name,
                             expr->type ? expr->type : type_int_get());
    }

    case AST_BINARY_EXPR: {
        IRVal lhs = gen_expr(g, expr->binary.lhs);
        IRVal rhs = gen_expr(g, expr->binary.rhs);
        int dst = new_temp(g);

        IROp op;
        switch (expr->binary.op) {
        case BIN_ADD: op = IR_ADD; break;
        case BIN_SUB: op = IR_SUB; break;
        case BIN_MUL: op = IR_MUL; break;
        case BIN_DIV: op = IR_DIV; break;
        case BIN_MOD: op = IR_MOD; break;
        case BIN_LSHIFT: op = IR_SHL; break;
        case BIN_RSHIFT: op = IR_SHR; break;
        case BIN_LT: op = IR_LT; break;
        case BIN_GT: op = IR_GT; break;
        case BIN_LE: op = IR_LE; break;
        case BIN_GE: op = IR_GE; break;
        case BIN_EQ: op = IR_EQ; break;
        case BIN_NE: op = IR_NE; break;
        case BIN_BIT_AND: op = IR_AND; break;
        case BIN_BIT_XOR: op = IR_XOR; break;
        case BIN_BIT_OR:  op = IR_OR;  break;
        case BIN_LOG_AND: {
            /* Short-circuit: a && b */
            int t = new_temp(g);
            int lbl_false = new_label(g);
            int lbl_end = new_label(g);

            /* If lhs is false, result is 0 */
            IRInstr *jz = ir_instr_new(g->arena, IR_JZ);
            jz->src1 = lhs;
            jz->label_id = lbl_false;
            emit(g, jz);

            /* Evaluate rhs */
            IRVal rhs_val = gen_expr(g, expr->binary.rhs);
            IRInstr *mov1 = ir_instr_new(g->arena, IR_MOV);
            mov1->dst = ir_val_temp(t, type_int_get());
            mov1->src1 = rhs_val;
            emit(g, mov1);

            IRInstr *jmp = ir_instr_new(g->arena, IR_JMP);
            jmp->label_id = lbl_end;
            emit(g, jmp);

            emit_label(g, lbl_false);
            IRInstr *mov0 = ir_instr_new(g->arena, IR_LOAD_IMM);
            mov0->dst = ir_val_temp(t, type_int_get());
            mov0->src1 = ir_val_imm_int(0, type_int_get());
            emit(g, mov0);

            emit_label(g, lbl_end);
            return ir_val_temp(t, type_int_get());
        }
        case BIN_LOG_OR: {
            /* Short-circuit: a || b */
            int t = new_temp(g);
            int lbl_true = new_label(g);
            int lbl_end = new_label(g);

            IRInstr *jnz = ir_instr_new(g->arena, IR_JNZ);
            jnz->src1 = lhs;
            jnz->label_id = lbl_true;
            emit(g, jnz);

            IRVal rhs_val = gen_expr(g, expr->binary.rhs);
            IRInstr *mov_rhs = ir_instr_new(g->arena, IR_MOV);
            mov_rhs->dst = ir_val_temp(t, type_int_get());
            mov_rhs->src1 = rhs_val;
            emit(g, mov_rhs);

            IRInstr *jmp = ir_instr_new(g->arena, IR_JMP);
            jmp->label_id = lbl_end;
            emit(g, jmp);

            emit_label(g, lbl_true);
            IRInstr *mov1 = ir_instr_new(g->arena, IR_LOAD_IMM);
            mov1->dst = ir_val_temp(t, type_int_get());
            mov1->src1 = ir_val_imm_int(1, type_int_get());
            emit(g, mov1);

            emit_label(g, lbl_end);
            return ir_val_temp(t, type_int_get());
        }
        }

        IRInstr *instr = ir_instr_new(g->arena, op);
        instr->dst = ir_val_temp(dst, expr->type ? expr->type : type_int_get());
        instr->src1 = lhs;
        instr->src2 = rhs;
        emit(g, instr);
        return instr->dst;
    }

    case AST_UNARY_EXPR: {
        IRVal operand = gen_expr(g, expr->unary.operand);
        int dst = new_temp(g);

        switch (expr->unary.op) {
        case UNARY_NEG: {
            IRInstr *i = ir_instr_new(g->arena, IR_NEG);
            i->dst = ir_val_temp(dst, expr->type);
            i->src1 = operand;
            emit(g, i);
            return i->dst;
        }
        case UNARY_NOT: {
            IRInstr *i = ir_instr_new(g->arena, IR_EQ);
            i->dst = ir_val_temp(dst, type_int_get());
            i->src1 = operand;
            i->src2 = ir_val_imm_int(0, type_int_get());
            emit(g, i);
            return i->dst;
        }
        case UNARY_BITNOT: {
            IRInstr *i = ir_instr_new(g->arena, IR_NOT);
            i->dst = ir_val_temp(dst, expr->type);
            i->src1 = operand;
            emit(g, i);
            return i->dst;
        }
        case UNARY_ADDR: {
            IRInstr *i = ir_instr_new(g->arena, IR_LEA);
            i->dst = ir_val_temp(dst, expr->type);
            i->src1 = operand;
            emit(g, i);
            return i->dst;
        }
        case UNARY_DEREF: {
            IRInstr *i = ir_instr_new(g->arena, IR_LOAD);
            i->dst = ir_val_temp(dst, expr->type);
            i->src1 = operand;
            emit(g, i);
            return i->dst;
        }
        case UNARY_PRE_INC: {
            IRInstr *add = ir_instr_new(g->arena, IR_ADD);
            add->dst = ir_val_temp(dst, expr->type);
            add->src1 = operand;
            add->src2 = ir_val_imm_int(1, type_int_get());
            emit(g, add);
            IRInstr *store = ir_instr_new(g->arena, IR_MOV);
            store->dst = operand;
            store->src1 = add->dst;
            emit(g, store);
            return add->dst;
        }
        case UNARY_PRE_DEC: {
            IRInstr *sub = ir_instr_new(g->arena, IR_SUB);
            sub->dst = ir_val_temp(dst, expr->type);
            sub->src1 = operand;
            sub->src2 = ir_val_imm_int(1, type_int_get());
            emit(g, sub);
            IRInstr *store = ir_instr_new(g->arena, IR_MOV);
            store->dst = operand;
            store->src1 = sub->dst;
            emit(g, store);
            return sub->dst;
        }
        default:
            return operand;
        }
    }

    case AST_ASSIGN_EXPR: {
        IRVal rhs = gen_expr(g, expr->assign.rhs);
        IRVal lhs = gen_expr(g, expr->assign.lhs);

        if (expr->assign.op == ASSIGN_EQ) {
            IRInstr *mov = ir_instr_new(g->arena, IR_MOV);
            mov->dst = lhs;
            mov->src1 = rhs;
            emit(g, mov);
            return rhs;
        } else {
            /* Compound assignment: a += b => a = a + b */
            int dst = new_temp(g);
            IROp op;
            switch (expr->assign.op) {
            case ASSIGN_ADD: op = IR_ADD; break;
            case ASSIGN_SUB: op = IR_SUB; break;
            case ASSIGN_MUL: op = IR_MUL; break;
            case ASSIGN_DIV: op = IR_DIV; break;
            case ASSIGN_MOD: op = IR_MOD; break;
            case ASSIGN_LSHIFT: op = IR_SHL; break;
            case ASSIGN_RSHIFT: op = IR_SHR; break;
            case ASSIGN_AND: op = IR_AND; break;
            case ASSIGN_XOR: op = IR_XOR; break;
            case ASSIGN_OR:  op = IR_OR; break;
            default: op = IR_ADD; break;
            }
            IRInstr *binop = ir_instr_new(g->arena, op);
            binop->dst = ir_val_temp(dst, expr->type);
            binop->src1 = lhs;
            binop->src2 = rhs;
            emit(g, binop);

            IRInstr *mov = ir_instr_new(g->arena, IR_MOV);
            mov->dst = lhs;
            mov->src1 = binop->dst;
            emit(g, mov);
            return binop->dst;
        }
    }

    case AST_CALL_EXPR: {
        /* Push arguments */
        for (size_t i = 0; i < expr->call.args.size; i++) {
            IRVal arg = gen_expr(g, (ASTNode *)vec_get(&expr->call.args, i));
            IRInstr *argi = ir_instr_new(g->arena, IR_ARG);
            argi->src1 = arg;
            emit(g, argi);
        }
        /* Call */
        IRVal callee = gen_expr(g, expr->call.callee);
        int dst = new_temp(g);
        IRInstr *call = ir_instr_new(g->arena, IR_CALL);
        call->dst = ir_val_temp(dst, expr->type ? expr->type : type_int_get());
        call->src1 = callee;
        emit(g, call);
        return call->dst;
    }

    case AST_CAST_EXPR: {
        IRVal operand = gen_expr(g, expr->cast.operand);
        /* Simplified: just move */
        int dst = new_temp(g);
        IRInstr *i = ir_instr_new(g->arena, IR_BITCAST);
        i->dst = ir_val_temp(dst, expr->cast.cast_type);
        i->src1 = operand;
        emit(g, i);
        return i->dst;
    }

    case AST_TERNARY_EXPR: {
        IRVal cond = gen_expr(g, expr->ternary.cond);
        int dst = new_temp(g);
        int lbl_false = new_label(g);
        int lbl_end = new_label(g);

        IRInstr *jz = ir_instr_new(g->arena, IR_JZ);
        jz->src1 = cond;
        jz->label_id = lbl_false;
        emit(g, jz);

        IRVal then_val = gen_expr(g, expr->ternary.then_expr);
        IRInstr *mov1 = ir_instr_new(g->arena, IR_MOV);
        mov1->dst = ir_val_temp(dst, expr->type);
        mov1->src1 = then_val;
        emit(g, mov1);

        IRInstr *jmp = ir_instr_new(g->arena, IR_JMP);
        jmp->label_id = lbl_end;
        emit(g, jmp);

        emit_label(g, lbl_false);
        IRVal else_val = gen_expr(g, expr->ternary.else_expr);
        IRInstr *mov2 = ir_instr_new(g->arena, IR_MOV);
        mov2->dst = ir_val_temp(dst, expr->type);
        mov2->src1 = else_val;
        emit(g, mov2);

        emit_label(g, lbl_end);
        return ir_val_temp(dst, expr->type);
    }

    case AST_SIZEOF_EXPR: {
        size_t sz = 0;
        if (expr->sizeof_expr.is_type && expr->sizeof_expr.sizeof_type) {
            sz = expr->sizeof_expr.sizeof_type->size;
        } else if (expr->sizeof_expr.sizeof_expr &&
                   expr->sizeof_expr.sizeof_expr->type) {
            sz = expr->sizeof_expr.sizeof_expr->type->size;
        }
        if (sz == 0) sz = 4; /* fallback */
        return ir_val_imm_int((long long)sz, type_ulong_get());
    }

    default:
        return ir_val_imm_int(0, type_int_get());
    }
}

/* ---- Statement IR generation ---- */

static void gen_stmt(IRGen *g, ASTNode *stmt) {
    if (!stmt) return;

    switch (stmt->kind) {
    case AST_COMPOUND_STMT:
        for (size_t i = 0; i < stmt->compound.stmts.size; i++) {
            ASTNode *child = (ASTNode *)vec_get(&stmt->compound.stmts, i);
            if (child->kind == AST_VAR_DECL)
                gen_decl(g, child);
            else
                gen_stmt(g, child);
        }
        break;

    case AST_EXPR_STMT:
        gen_expr(g, stmt->expr_stmt.expr);
        break;

    case AST_RETURN_STMT: {
        IRInstr *ret = ir_instr_new(g->arena, IR_RET);
        if (stmt->return_stmt.expr)
            ret->src1 = gen_expr(g, stmt->return_stmt.expr);
        emit(g, ret);
        break;
    }

    case AST_IF_STMT: {
        IRVal cond = gen_expr(g, stmt->if_stmt.cond);
        int lbl_else = new_label(g);
        int lbl_end = new_label(g);

        IRInstr *jz = ir_instr_new(g->arena, IR_JZ);
        jz->src1 = cond;
        jz->label_id = stmt->if_stmt.else_stmt ? lbl_else : lbl_end;
        emit(g, jz);

        gen_stmt(g, stmt->if_stmt.then_stmt);

        if (stmt->if_stmt.else_stmt) {
            IRInstr *jmp = ir_instr_new(g->arena, IR_JMP);
            jmp->label_id = lbl_end;
            emit(g, jmp);

            emit_label(g, lbl_else);
            gen_stmt(g, stmt->if_stmt.else_stmt);
        }
        emit_label(g, lbl_end);
        break;
    }

    case AST_WHILE_STMT: {
        int lbl_cond = new_label(g);
        int lbl_end = new_label(g);
        int prev_break = g->break_label;
        int prev_continue = g->continue_label;
        g->break_label = lbl_end;
        g->continue_label = lbl_cond;

        emit_label(g, lbl_cond);
        IRVal cond = gen_expr(g, stmt->while_stmt.cond);
        IRInstr *jz = ir_instr_new(g->arena, IR_JZ);
        jz->src1 = cond;
        jz->label_id = lbl_end;
        emit(g, jz);

        gen_stmt(g, stmt->while_stmt.body);

        IRInstr *jmp = ir_instr_new(g->arena, IR_JMP);
        jmp->label_id = lbl_cond;
        emit(g, jmp);

        emit_label(g, lbl_end);
        g->break_label = prev_break;
        g->continue_label = prev_continue;
        break;
    }

    case AST_DO_WHILE_STMT: {
        int lbl_body = new_label(g);
        int lbl_cond = new_label(g);
        int lbl_end = new_label(g);
        int prev_break = g->break_label;
        int prev_continue = g->continue_label;
        g->break_label = lbl_end;
        g->continue_label = lbl_cond;

        emit_label(g, lbl_body);
        gen_stmt(g, stmt->do_while_stmt.body);

        emit_label(g, lbl_cond);
        IRVal cond = gen_expr(g, stmt->do_while_stmt.cond);
        IRInstr *jnz = ir_instr_new(g->arena, IR_JNZ);
        jnz->src1 = cond;
        jnz->label_id = lbl_body;
        emit(g, jnz);

        emit_label(g, lbl_end);
        g->break_label = prev_break;
        g->continue_label = prev_continue;
        break;
    }

    case AST_FOR_STMT: {
        int lbl_cond = new_label(g);
        int lbl_incr = new_label(g);
        int lbl_end = new_label(g);
        int prev_break = g->break_label;
        int prev_continue = g->continue_label;
        g->break_label = lbl_end;
        g->continue_label = lbl_incr;

        if (stmt->for_stmt.init) {
            if (stmt->for_stmt.init->kind == AST_VAR_DECL)
                gen_decl(g, stmt->for_stmt.init);
            else
                gen_expr(g, stmt->for_stmt.init);
        }

        emit_label(g, lbl_cond);
        if (stmt->for_stmt.cond) {
            IRVal cond = gen_expr(g, stmt->for_stmt.cond);
            IRInstr *jz = ir_instr_new(g->arena, IR_JZ);
            jz->src1 = cond;
            jz->label_id = lbl_end;
            emit(g, jz);
        }

        gen_stmt(g, stmt->for_stmt.body);

        emit_label(g, lbl_incr);
        if (stmt->for_stmt.incr)
            gen_expr(g, stmt->for_stmt.incr);

        IRInstr *jmp = ir_instr_new(g->arena, IR_JMP);
        jmp->label_id = lbl_cond;
        emit(g, jmp);

        emit_label(g, lbl_end);
        g->break_label = prev_break;
        g->continue_label = prev_continue;
        break;
    }

    case AST_BREAK_STMT: {
        if (g->break_label >= 0) {
            IRInstr *jmp = ir_instr_new(g->arena, IR_JMP);
            jmp->label_id = g->break_label;
            emit(g, jmp);
        }
        break;
    }

    case AST_CONTINUE_STMT: {
        if (g->continue_label >= 0) {
            IRInstr *jmp = ir_instr_new(g->arena, IR_JMP);
            jmp->label_id = g->continue_label;
            emit(g, jmp);
        }
        break;
    }

    case AST_LABEL_STMT: {
        int lbl = new_label(g);
        emit_label(g, lbl);
        gen_stmt(g, stmt->label_stmt.stmt);
        break;
    }

    case AST_SWITCH_STMT:
    case AST_CASE_STMT:
    case AST_DEFAULT_STMT:
        /* TODO: implement switch/case IR generation */
        break;

    case AST_NULL_STMT:
        break;

    default:
        break;
    }
}

/* ---- Declaration IR generation ---- */

static void gen_decl(IRGen *g, ASTNode *decl) {
    if (!decl) return;

    if (decl->kind == AST_VAR_DECL && decl->var_decl.name) {
        /* Allocate a temp for this variable */
        int slot = new_temp(g);
        Symbol *sym = symtab_lookup(g->symtab, decl->var_decl.name);
        if (sym) sym->ir_slot = slot;

        /* Stack allocation */
        IRInstr *alloca_i = ir_instr_new(g->arena, IR_ALLOCA);
        alloca_i->dst = ir_val_temp(slot,
            decl->var_decl.var_type ? decl->var_decl.var_type : type_int_get());
        size_t sz = decl->var_decl.var_type ? decl->var_decl.var_type->size : 4;
        if (sz == 0) sz = 4;
        alloca_i->src1 = ir_val_imm_int((long long)sz, type_int_get());
        emit(g, alloca_i);

        /* Initialize if needed */
        if (decl->var_decl.init) {
            IRVal init_val = gen_expr(g, decl->var_decl.init);
            IRInstr *mov = ir_instr_new(g->arena, IR_MOV);
            mov->dst = ir_val_temp(slot, decl->var_decl.var_type);
            mov->src1 = init_val;
            emit(g, mov);
        }
    }
}

/* ---- Top-level function generation ---- */

static void gen_function(IRGen *g, ASTNode *func) {
    IRFunc *irfunc = ir_func_new(g->arena, func->func_def.name, func->type);
    g->current_func = irfunc;
    g->temp_counter = 0;
    g->label_counter = 0;

    /* Create entry block */
    IRBlock *entry = ir_block_new(g->arena, new_label(g));
    vec_push(&irfunc->blocks, entry);
    g->current_block = entry;

    /* Function begin marker */
    IRInstr *begin = ir_instr_new(g->arena, IR_FUNC_BEGIN);
    emit(g, begin);

    /* Allocate temps for parameters */
    for (size_t i = 0; i < func->func_def.params.size; i++) {
        ASTNode *pd = (ASTNode *)vec_get(&func->func_def.params, i);
        if (pd->param_decl.name) {
            int slot = new_temp(g);
            Symbol *sym = symtab_lookup(g->symtab, pd->param_decl.name);
            if (sym) sym->ir_slot = slot;
        }
    }

    /* Generate body */
    gen_stmt(g, func->func_def.body);

    /* Function end marker */
    IRInstr *end = ir_instr_new(g->arena, IR_FUNC_END);
    emit(g, end);

    irfunc->temp_count = g->temp_counter;
    irfunc->label_count = g->label_counter;

    vec_push(&g->module->functions, irfunc);
    g->current_func = NULL;
    g->current_block = NULL;
}

/* ---- Entry point ---- */

IRModule *ir_gen_translate(IRGen *g, ASTNode *tu) {
    if (!tu || tu->kind != AST_TRANSLATION_UNIT) return g->module;

    for (size_t i = 0; i < tu->tu.decls.size; i++) {
        ASTNode *decl = (ASTNode *)vec_get(&tu->tu.decls, i);
        if (decl->kind == AST_FUNC_DEF) {
            gen_function(g, decl);
        }
        /* Global variable declarations handled separately */
    }

    return g->module;
}
