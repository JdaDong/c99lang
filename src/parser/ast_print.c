/*
 * c99lang - AST pretty printer
 */
#include "parser/ast.h"
#include <stdio.h>

static const char *ast_kind_names[AST_KIND_COUNT] = {
    [AST_TRANSLATION_UNIT] = "TranslationUnit",
    [AST_FUNC_DEF]         = "FuncDef",
    [AST_VAR_DECL]         = "VarDecl",
    [AST_PARAM_DECL]       = "ParamDecl",
    [AST_TYPEDEF_DECL]     = "TypedefDecl",
    [AST_STRUCT_DECL]      = "StructDecl",
    [AST_UNION_DECL]       = "UnionDecl",
    [AST_ENUM_DECL]        = "EnumDecl",
    [AST_ENUM_CONST]       = "EnumConst",
    [AST_FIELD_DECL]       = "FieldDecl",
    [AST_COMPOUND_STMT]    = "CompoundStmt",
    [AST_EXPR_STMT]        = "ExprStmt",
    [AST_IF_STMT]          = "IfStmt",
    [AST_WHILE_STMT]       = "WhileStmt",
    [AST_DO_WHILE_STMT]    = "DoWhileStmt",
    [AST_FOR_STMT]         = "ForStmt",
    [AST_SWITCH_STMT]      = "SwitchStmt",
    [AST_CASE_STMT]        = "CaseStmt",
    [AST_DEFAULT_STMT]     = "DefaultStmt",
    [AST_RETURN_STMT]      = "ReturnStmt",
    [AST_BREAK_STMT]       = "BreakStmt",
    [AST_CONTINUE_STMT]    = "ContinueStmt",
    [AST_GOTO_STMT]        = "GotoStmt",
    [AST_LABEL_STMT]       = "LabelStmt",
    [AST_NULL_STMT]        = "NullStmt",
    [AST_INT_LIT]          = "IntLit",
    [AST_FLOAT_LIT]        = "FloatLit",
    [AST_CHAR_LIT]         = "CharLit",
    [AST_STRING_LIT]       = "StringLit",
    [AST_IDENT_EXPR]       = "IdentExpr",
    [AST_BINARY_EXPR]      = "BinaryExpr",
    [AST_UNARY_EXPR]       = "UnaryExpr",
    [AST_POSTFIX_EXPR]     = "PostfixExpr",
    [AST_TERNARY_EXPR]     = "TernaryExpr",
    [AST_CALL_EXPR]        = "CallExpr",
    [AST_INDEX_EXPR]       = "IndexExpr",
    [AST_MEMBER_EXPR]      = "MemberExpr",
    [AST_CAST_EXPR]        = "CastExpr",
    [AST_SIZEOF_EXPR]      = "SizeofExpr",
    [AST_COMPOUND_LIT]     = "CompoundLit",
    [AST_INIT_LIST]        = "InitList",
    [AST_DESIGNATOR]       = "Designator",
    [AST_ASSIGN_EXPR]      = "AssignExpr",
    [AST_COMMA_EXPR]       = "CommaExpr",
};

const char *ast_kind_str(ASTKind kind) {
    if (kind >= 0 && kind < AST_KIND_COUNT && ast_kind_names[kind])
        return ast_kind_names[kind];
    return "???";
}

const char *binop_str(BinOp op) {
    switch (op) {
    case BIN_ADD: return "+"; case BIN_SUB: return "-";
    case BIN_MUL: return "*"; case BIN_DIV: return "/";
    case BIN_MOD: return "%";
    case BIN_LSHIFT: return "<<"; case BIN_RSHIFT: return ">>";
    case BIN_LT: return "<"; case BIN_GT: return ">";
    case BIN_LE: return "<="; case BIN_GE: return ">=";
    case BIN_EQ: return "=="; case BIN_NE: return "!=";
    case BIN_BIT_AND: return "&"; case BIN_BIT_XOR: return "^";
    case BIN_BIT_OR: return "|";
    case BIN_LOG_AND: return "&&"; case BIN_LOG_OR: return "||";
    }
    return "?";
}

const char *unaryop_str(UnaryOp op) {
    switch (op) {
    case UNARY_NEG: return "-"; case UNARY_POS: return "+";
    case UNARY_NOT: return "!"; case UNARY_BITNOT: return "~";
    case UNARY_DEREF: return "*"; case UNARY_ADDR: return "&";
    case UNARY_PRE_INC: return "++"; case UNARY_PRE_DEC: return "--";
    }
    return "?";
}

const char *assignop_str(AssignOp op) {
    switch (op) {
    case ASSIGN_EQ: return "=";
    case ASSIGN_MUL: return "*="; case ASSIGN_DIV: return "/=";
    case ASSIGN_MOD: return "%=";
    case ASSIGN_ADD: return "+="; case ASSIGN_SUB: return "-=";
    case ASSIGN_LSHIFT: return "<<="; case ASSIGN_RSHIFT: return ">>=";
    case ASSIGN_AND: return "&="; case ASSIGN_XOR: return "^=";
    case ASSIGN_OR: return "|=";
    }
    return "?";
}

static void indent(int n) {
    for (int i = 0; i < n; i++) printf("  ");
}

void ast_print(const ASTNode *node, int ind) {
    if (!node) {
        indent(ind); printf("(null)\n");
        return;
    }

    indent(ind);
    printf("%s", ast_kind_str(node->kind));

    switch (node->kind) {
    case AST_TRANSLATION_UNIT:
        printf(" (%zu decls)\n", node->tu.decls.size);
        for (size_t i = 0; i < node->tu.decls.size; i++)
            ast_print((ASTNode *)vec_get(&node->tu.decls, i), ind + 1);
        break;

    case AST_FUNC_DEF:
        printf(" '%s' (%zu params)\n",
               node->func_def.name ? node->func_def.name : "<anon>",
               node->func_def.params.size);
        ast_print(node->func_def.body, ind + 1);
        break;

    case AST_VAR_DECL:
        printf(" '%s'\n", node->var_decl.name ? node->var_decl.name : "<anon>");
        if (node->var_decl.init)
            ast_print(node->var_decl.init, ind + 1);
        break;

    case AST_COMPOUND_STMT:
        printf(" (%zu stmts)\n", node->compound.stmts.size);
        for (size_t i = 0; i < node->compound.stmts.size; i++)
            ast_print((ASTNode *)vec_get(&node->compound.stmts, i), ind + 1);
        break;

    case AST_RETURN_STMT:
        printf("\n");
        if (node->return_stmt.expr)
            ast_print(node->return_stmt.expr, ind + 1);
        break;

    case AST_IF_STMT:
        printf("\n");
        indent(ind + 1); printf("cond:\n");
        ast_print(node->if_stmt.cond, ind + 2);
        indent(ind + 1); printf("then:\n");
        ast_print(node->if_stmt.then_stmt, ind + 2);
        if (node->if_stmt.else_stmt) {
            indent(ind + 1); printf("else:\n");
            ast_print(node->if_stmt.else_stmt, ind + 2);
        }
        break;

    case AST_WHILE_STMT:
        printf("\n");
        ast_print(node->while_stmt.cond, ind + 1);
        ast_print(node->while_stmt.body, ind + 1);
        break;

    case AST_FOR_STMT:
        printf("\n");
        indent(ind + 1); printf("init:\n");
        ast_print(node->for_stmt.init, ind + 2);
        indent(ind + 1); printf("cond:\n");
        ast_print(node->for_stmt.cond, ind + 2);
        indent(ind + 1); printf("incr:\n");
        ast_print(node->for_stmt.incr, ind + 2);
        indent(ind + 1); printf("body:\n");
        ast_print(node->for_stmt.body, ind + 2);
        break;

    case AST_INT_LIT:
        printf(" %llu\n", node->int_lit.value);
        break;

    case AST_FLOAT_LIT:
        printf(" %Lg\n", node->float_lit.value);
        break;

    case AST_STRING_LIT:
        printf(" %.*s\n", (int)node->string_lit.length, node->string_lit.value);
        break;

    case AST_IDENT_EXPR:
        printf(" '%s'\n", node->ident.name);
        break;

    case AST_BINARY_EXPR:
        printf(" '%s'\n", binop_str(node->binary.op));
        ast_print(node->binary.lhs, ind + 1);
        ast_print(node->binary.rhs, ind + 1);
        break;

    case AST_UNARY_EXPR:
        printf(" '%s'\n", unaryop_str(node->unary.op));
        ast_print(node->unary.operand, ind + 1);
        break;

    case AST_ASSIGN_EXPR:
        printf(" '%s'\n", assignop_str(node->assign.op));
        ast_print(node->assign.lhs, ind + 1);
        ast_print(node->assign.rhs, ind + 1);
        break;

    case AST_CALL_EXPR:
        printf(" (%zu args)\n", node->call.args.size);
        ast_print(node->call.callee, ind + 1);
        for (size_t i = 0; i < node->call.args.size; i++)
            ast_print((ASTNode *)vec_get(&node->call.args, i), ind + 1);
        break;

    case AST_EXPR_STMT:
        printf("\n");
        ast_print(node->expr_stmt.expr, ind + 1);
        break;

    default:
        printf("\n");
        break;
    }
}
