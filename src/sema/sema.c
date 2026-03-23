/*
 * c99lang - Semantic analysis
 * Type checking, symbol resolution, implicit conversions
 */
#include "sema/sema.h"
#include "lexer/token.h"
#include <string.h>
#include <stdio.h>

void sema_init(Sema *s, Arena *arena, ErrorCtx *err) {
    s->arena = arena;
    symtab_init(&s->symtab, arena);
    s->err = err;
    s->current_func_ret = NULL;
    s->loop_depth = 0;
    s->switch_depth = 0;
}

/* ---- Integer promotion (C99 6.3.1.1) ---- */

CType *sema_integer_promotions(CType *t) {
    if (!t) return type_int_get();
    switch (t->kind) {
    case TYPE_BOOL: case TYPE_CHAR: case TYPE_SCHAR: case TYPE_UCHAR:
    case TYPE_SHORT: case TYPE_USHORT:
        return type_int_get();
    default:
        return t;
    }
}

/* ---- Usual arithmetic conversions (C99 6.3.1.8) ---- */

CType *sema_usual_arithmetic_conversions(CType *a, CType *b) {
    if (!a || !b) return type_int_get();

    /* If either is long double, convert to long double */
    if (a->kind == TYPE_LDOUBLE || b->kind == TYPE_LDOUBLE)
        return type_new(NULL, TYPE_LDOUBLE);
    if (a->kind == TYPE_DOUBLE || b->kind == TYPE_DOUBLE)
        return type_double_get();
    if (a->kind == TYPE_FLOAT || b->kind == TYPE_FLOAT)
        return type_float_get();

    /* Integer promotions */
    a = sema_integer_promotions(a);
    b = sema_integer_promotions(b);

    /* Same type */
    if (a->kind == b->kind) return a;

    /* Conversion rank */
    if (a->kind == TYPE_ULLONG || b->kind == TYPE_ULLONG) return type_ullong_get();
    if (a->kind == TYPE_LLONG  || b->kind == TYPE_LLONG)  return type_llong_get();
    if (a->kind == TYPE_ULONG  || b->kind == TYPE_ULONG)  return type_ulong_get();
    if (a->kind == TYPE_LONG   || b->kind == TYPE_LONG)    return type_long_get();
    if (a->kind == TYPE_UINT   || b->kind == TYPE_UINT)    return type_uint_get();

    return type_int_get();
}

/* ---- Expression type checking ---- */

CType *sema_check_expr(Sema *s, ASTNode *expr) {
    if (!expr) return NULL;

    switch (expr->kind) {
    case AST_INT_LIT: {
        CType *t;
        switch (expr->int_lit.suffix) {
        case INT_SUFFIX_ULL: t = type_ullong_get(); break;
        case INT_SUFFIX_LL:  t = type_llong_get(); break;
        case INT_SUFFIX_UL:  t = type_ulong_get(); break;
        case INT_SUFFIX_L:   t = type_long_get(); break;
        case INT_SUFFIX_U:   t = type_uint_get(); break;
        default:             t = type_int_get(); break;
        }
        expr->type = t;
        return t;
    }

    case AST_FLOAT_LIT: {
        CType *t;
        switch (expr->float_lit.suffix) {
        case FLOAT_SUFFIX_F: t = type_float_get(); break;
        case FLOAT_SUFFIX_L: t = type_new(s->arena, TYPE_LDOUBLE); break;
        default:             t = type_double_get(); break;
        }
        expr->type = t;
        return t;
    }

    case AST_CHAR_LIT:
        expr->type = type_int_get(); /* char literals are int in C */
        return expr->type;

    case AST_STRING_LIT:
        expr->type = type_ptr(s->arena, type_char_get(), QUAL_CONST);
        return expr->type;

    case AST_IDENT_EXPR: {
        Symbol *sym = symtab_lookup(&s->symtab, expr->ident.name);
        if (!sym) {
            diag_error(s->err, expr->loc, "undeclared identifier '%s'",
                       expr->ident.name);
            expr->type = type_int_get();
        } else {
            expr->type = sym->type;
        }
        return expr->type;
    }

    case AST_BINARY_EXPR: {
        CType *lt = sema_check_expr(s, expr->binary.lhs);
        CType *rt = sema_check_expr(s, expr->binary.rhs);

        switch (expr->binary.op) {
        case BIN_ADD: case BIN_SUB: case BIN_MUL: case BIN_DIV: case BIN_MOD:
            /* Pointer arithmetic */
            if (lt && lt->kind == TYPE_POINTER &&
                (expr->binary.op == BIN_ADD || expr->binary.op == BIN_SUB)) {
                expr->type = lt;
            } else {
                expr->type = sema_usual_arithmetic_conversions(lt, rt);
            }
            break;

        case BIN_LT: case BIN_GT: case BIN_LE: case BIN_GE:
        case BIN_EQ: case BIN_NE:
        case BIN_LOG_AND: case BIN_LOG_OR:
            expr->type = type_int_get();
            break;

        case BIN_LSHIFT: case BIN_RSHIFT:
        case BIN_BIT_AND: case BIN_BIT_XOR: case BIN_BIT_OR:
            expr->type = sema_usual_arithmetic_conversions(lt, rt);
            break;
        }
        return expr->type;
    }

    case AST_UNARY_EXPR: {
        CType *ot = sema_check_expr(s, expr->unary.operand);
        switch (expr->unary.op) {
        case UNARY_ADDR:
            expr->type = type_ptr(s->arena, ot ? ot : type_int_get(), 0);
            break;
        case UNARY_DEREF:
            if (ot && ot->kind == TYPE_POINTER)
                expr->type = ot->ptr.pointee;
            else {
                diag_error(s->err, expr->loc,
                           "dereference of non-pointer type");
                expr->type = type_int_get();
            }
            break;
        case UNARY_NOT:
            expr->type = type_int_get();
            break;
        default:
            expr->type = ot ? ot : type_int_get();
            break;
        }
        return expr->type;
    }

    case AST_POSTFIX_EXPR: {
        CType *ot = sema_check_expr(s, expr->postfix.operand);
        expr->type = ot ? ot : type_int_get();
        return expr->type;
    }

    case AST_ASSIGN_EXPR: {
        CType *lt = sema_check_expr(s, expr->assign.lhs);
        sema_check_expr(s, expr->assign.rhs);
        expr->type = lt ? lt : type_int_get();
        return expr->type;
    }

    case AST_CALL_EXPR: {
        CType *ct = sema_check_expr(s, expr->call.callee);
        for (size_t i = 0; i < expr->call.args.size; i++) {
            sema_check_expr(s, (ASTNode *)vec_get(&expr->call.args, i));
        }
        if (ct && ct->kind == TYPE_FUNC)
            expr->type = ct->func.ret;
        else if (ct && ct->kind == TYPE_POINTER && ct->ptr.pointee &&
                 ct->ptr.pointee->kind == TYPE_FUNC)
            expr->type = ct->ptr.pointee->func.ret;
        else
            expr->type = type_int_get();
        return expr->type;
    }

    case AST_INDEX_EXPR: {
        CType *at = sema_check_expr(s, expr->index_expr.array);
        sema_check_expr(s, expr->index_expr.index);
        if (at && at->kind == TYPE_POINTER)
            expr->type = at->ptr.pointee;
        else if (at && at->kind == TYPE_ARRAY)
            expr->type = at->array.elem;
        else
            expr->type = type_int_get();
        return expr->type;
    }

    case AST_MEMBER_EXPR: {
        sema_check_expr(s, expr->member.object);
        /* TODO: look up field in struct/union */
        expr->type = type_int_get(); /* placeholder */
        return expr->type;
    }

    case AST_CAST_EXPR: {
        sema_check_expr(s, expr->cast.operand);
        expr->type = expr->cast.cast_type;
        return expr->type;
    }

    case AST_SIZEOF_EXPR:
        if (!expr->sizeof_expr.is_type) {
            sema_check_expr(s, expr->sizeof_expr.sizeof_expr);
        }
        expr->type = type_ulong_get(); /* size_t */
        return expr->type;

    case AST_TERNARY_EXPR: {
        sema_check_expr(s, expr->ternary.cond);
        CType *tt = sema_check_expr(s, expr->ternary.then_expr);
        CType *et = sema_check_expr(s, expr->ternary.else_expr);
        expr->type = sema_usual_arithmetic_conversions(tt, et);
        return expr->type;
    }

    case AST_COMMA_EXPR: {
        sema_check_expr(s, expr->comma.lhs);
        CType *rt = sema_check_expr(s, expr->comma.rhs);
        expr->type = rt;
        return expr->type;
    }

    default:
        expr->type = type_int_get();
        return expr->type;
    }
}

/* ---- Statement checking ---- */

void sema_check_stmt(Sema *s, ASTNode *stmt) {
    if (!stmt) return;

    switch (stmt->kind) {
    case AST_COMPOUND_STMT:
        symtab_push_scope(&s->symtab);
        for (size_t i = 0; i < stmt->compound.stmts.size; i++) {
            ASTNode *child = (ASTNode *)vec_get(&stmt->compound.stmts, i);
            if (child->kind == AST_VAR_DECL || child->kind == AST_FUNC_DEF)
                sema_check_decl(s, child);
            else
                sema_check_stmt(s, child);
        }
        symtab_pop_scope(&s->symtab);
        break;

    case AST_EXPR_STMT:
        sema_check_expr(s, stmt->expr_stmt.expr);
        break;

    case AST_IF_STMT:
        sema_check_expr(s, stmt->if_stmt.cond);
        sema_check_stmt(s, stmt->if_stmt.then_stmt);
        sema_check_stmt(s, stmt->if_stmt.else_stmt);
        break;

    case AST_WHILE_STMT:
        sema_check_expr(s, stmt->while_stmt.cond);
        s->loop_depth++;
        sema_check_stmt(s, stmt->while_stmt.body);
        s->loop_depth--;
        break;

    case AST_DO_WHILE_STMT:
        s->loop_depth++;
        sema_check_stmt(s, stmt->do_while_stmt.body);
        s->loop_depth--;
        sema_check_expr(s, stmt->do_while_stmt.cond);
        break;

    case AST_FOR_STMT:
        symtab_push_scope(&s->symtab);
        if (stmt->for_stmt.init) {
            if (stmt->for_stmt.init->kind == AST_VAR_DECL)
                sema_check_decl(s, stmt->for_stmt.init);
            else
                sema_check_expr(s, stmt->for_stmt.init);
        }
        sema_check_expr(s, stmt->for_stmt.cond);
        sema_check_expr(s, stmt->for_stmt.incr);
        s->loop_depth++;
        sema_check_stmt(s, stmt->for_stmt.body);
        s->loop_depth--;
        symtab_pop_scope(&s->symtab);
        break;

    case AST_SWITCH_STMT:
        sema_check_expr(s, stmt->switch_stmt.expr);
        s->switch_depth++;
        sema_check_stmt(s, stmt->switch_stmt.body);
        s->switch_depth--;
        break;

    case AST_CASE_STMT:
        if (s->switch_depth <= 0)
            diag_error(s->err, stmt->loc, "'case' outside of switch");
        sema_check_expr(s, stmt->case_stmt.value);
        sema_check_stmt(s, stmt->case_stmt.stmt);
        break;

    case AST_DEFAULT_STMT:
        if (s->switch_depth <= 0)
            diag_error(s->err, stmt->loc, "'default' outside of switch");
        sema_check_stmt(s, stmt->default_stmt.stmt);
        break;

    case AST_RETURN_STMT: {
        CType *rt = NULL;
        if (stmt->return_stmt.expr)
            rt = sema_check_expr(s, stmt->return_stmt.expr);
        if (s->current_func_ret && s->current_func_ret->kind == TYPE_VOID) {
            if (rt)
                diag_warn(s->err, stmt->loc,
                          "returning a value from void function");
        }
        break;
    }

    case AST_BREAK_STMT:
        if (s->loop_depth <= 0 && s->switch_depth <= 0)
            diag_error(s->err, stmt->loc, "'break' outside of loop or switch");
        break;

    case AST_CONTINUE_STMT:
        if (s->loop_depth <= 0)
            diag_error(s->err, stmt->loc, "'continue' outside of loop");
        break;

    case AST_LABEL_STMT:
        sema_check_stmt(s, stmt->label_stmt.stmt);
        break;

    case AST_GOTO_STMT:
        /* Label resolution could be done in a second pass */
        break;

    case AST_NULL_STMT:
        break;

    default:
        break;
    }
}

/* ---- Declaration checking ---- */

void sema_check_decl(Sema *s, ASTNode *decl) {
    if (!decl) return;

    switch (decl->kind) {
    case AST_VAR_DECL:
        if (decl->var_decl.name) {
            Symbol *existing = symtab_lookup_current_scope(
                &s->symtab, decl->var_decl.name);
            if (existing) {
                diag_error(s->err, decl->loc,
                           "redefinition of '%s'", decl->var_decl.name);
                diag_note(s->err, existing->loc,
                          "previous definition was here");
            } else {
                SymKind sk = (decl->var_decl.sc == SC_TYPEDEF) ?
                             SYM_TYPEDEF : SYM_VAR;
                symtab_insert(&s->symtab, decl->var_decl.name, sk,
                              decl->var_decl.var_type, decl->loc);
            }
        }
        if (decl->var_decl.init)
            sema_check_expr(s, decl->var_decl.init);
        break;

    case AST_FUNC_DEF: {
        /* Register function in symbol table */
        if (decl->func_def.name) {
            Symbol *sym = symtab_insert(&s->symtab, decl->func_def.name,
                                         SYM_FUNC, decl->type, decl->loc);
            sym->is_defined = true;
            if (decl->func_def.sc == SC_STATIC)
                sym->linkage = LINK_INTERNAL;
            else
                sym->linkage = LINK_EXTERNAL;
        }

        /* Enter function scope */
        CType *prev_ret = s->current_func_ret;
        s->current_func_ret = decl->func_def.ret_type;

        symtab_push_scope(&s->symtab);

        /* Register parameters */
        for (size_t i = 0; i < decl->func_def.params.size; i++) {
            ASTNode *pd = (ASTNode *)vec_get(&decl->func_def.params, i);
            if (pd->param_decl.name) {
                symtab_insert(&s->symtab, pd->param_decl.name, SYM_PARAM,
                              pd->param_decl.param_type, pd->loc);
            }
        }

        /* Check body */
        sema_check_stmt(s, decl->func_def.body);

        symtab_pop_scope(&s->symtab);
        s->current_func_ret = prev_ret;
        break;
    }

    default:
        break;
    }
}

/* ---- Top-level entry ---- */

void sema_check(Sema *s, ASTNode *tu) {
    if (!tu || tu->kind != AST_TRANSLATION_UNIT) return;

    for (size_t i = 0; i < tu->tu.decls.size; i++) {
        ASTNode *decl = (ASTNode *)vec_get(&tu->tu.decls, i);
        sema_check_decl(s, decl);
    }
}
