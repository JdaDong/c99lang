/*
 * c99lang - Parser: Statement parsing
 */
#include "parser/parser.h"
#include <string.h>

/* Helpers duplicated for this translation unit */
static void advance(Parser *p) { p->tok = lexer_next(p->lex); }

static bool check(Parser *p, TokenKind kind) {
    return p->tok.kind == kind;
}

static bool match(Parser *p, TokenKind kind) {
    if (p->tok.kind == kind) { advance(p); return true; }
    return false;
}

static Token expect(Parser *p, TokenKind kind) {
    Token t = p->tok;
    if (t.kind != kind) {
        diag_error(p->err, t.loc, "expected '%s', got '%s'",
                   token_kind_str(kind), token_kind_str(t.kind));
    }
    advance(p);
    return t;
}

/* ---- Statement parsing ---- */

ASTNode *parse_compound_statement(Parser *p) {
    SrcLoc loc = p->tok.loc;
    expect(p, TOK_LBRACE);

    ASTNode *comp = ast_new(p->arena, AST_COMPOUND_STMT, loc);
    vec_init(&comp->compound.stmts);

    while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
        ASTNode *stmt;
        if (parser_is_typename(p)) {
            stmt = parse_declaration(p);
        } else {
            stmt = parse_statement(p);
        }
        if (stmt) {
            vec_push(&comp->compound.stmts, stmt);
        }
    }

    expect(p, TOK_RBRACE);
    return comp;
}

ASTNode *parse_statement(Parser *p) {
    SrcLoc loc = p->tok.loc;

    /* Compound statement */
    if (check(p, TOK_LBRACE)) {
        return parse_compound_statement(p);
    }

    /* If statement */
    if (match(p, TOK_KW_IF)) {
        ASTNode *node = ast_new(p->arena, AST_IF_STMT, loc);
        expect(p, TOK_LPAREN);
        node->if_stmt.cond = parse_expression(p);
        expect(p, TOK_RPAREN);
        node->if_stmt.then_stmt = parse_statement(p);
        node->if_stmt.else_stmt = NULL;
        if (match(p, TOK_KW_ELSE)) {
            node->if_stmt.else_stmt = parse_statement(p);
        }
        return node;
    }

    /* While statement */
    if (match(p, TOK_KW_WHILE)) {
        ASTNode *node = ast_new(p->arena, AST_WHILE_STMT, loc);
        expect(p, TOK_LPAREN);
        node->while_stmt.cond = parse_expression(p);
        expect(p, TOK_RPAREN);
        node->while_stmt.body = parse_statement(p);
        return node;
    }

    /* Do-while statement */
    if (match(p, TOK_KW_DO)) {
        ASTNode *node = ast_new(p->arena, AST_DO_WHILE_STMT, loc);
        node->do_while_stmt.body = parse_statement(p);
        expect(p, TOK_KW_WHILE);
        expect(p, TOK_LPAREN);
        node->do_while_stmt.cond = parse_expression(p);
        expect(p, TOK_RPAREN);
        expect(p, TOK_SEMICOLON);
        return node;
    }

    /* For statement (C99: allows declaration in init) */
    if (match(p, TOK_KW_FOR)) {
        ASTNode *node = ast_new(p->arena, AST_FOR_STMT, loc);
        expect(p, TOK_LPAREN);

        /* Init: declaration or expression or empty */
        if (check(p, TOK_SEMICOLON)) {
            node->for_stmt.init = NULL;
            advance(p);
        } else if (parser_is_typename(p)) {
            /* C99 for-loop declaration */
            node->for_stmt.init = parse_declaration(p);
            /* Note: parse_declaration already consumed the semicolon */
        } else {
            node->for_stmt.init = parse_expression(p);
            expect(p, TOK_SEMICOLON);
        }

        /* Condition */
        if (check(p, TOK_SEMICOLON)) {
            node->for_stmt.cond = NULL;
        } else {
            node->for_stmt.cond = parse_expression(p);
        }
        expect(p, TOK_SEMICOLON);

        /* Increment */
        if (check(p, TOK_RPAREN)) {
            node->for_stmt.incr = NULL;
        } else {
            node->for_stmt.incr = parse_expression(p);
        }
        expect(p, TOK_RPAREN);

        node->for_stmt.body = parse_statement(p);
        return node;
    }

    /* Switch statement */
    if (match(p, TOK_KW_SWITCH)) {
        ASTNode *node = ast_new(p->arena, AST_SWITCH_STMT, loc);
        expect(p, TOK_LPAREN);
        node->switch_stmt.expr = parse_expression(p);
        expect(p, TOK_RPAREN);
        node->switch_stmt.body = parse_statement(p);
        return node;
    }

    /* Case */
    if (match(p, TOK_KW_CASE)) {
        ASTNode *node = ast_new(p->arena, AST_CASE_STMT, loc);
        node->case_stmt.value = parse_constant_expression(p);
        expect(p, TOK_COLON);
        node->case_stmt.stmt = parse_statement(p);
        return node;
    }

    /* Default */
    if (match(p, TOK_KW_DEFAULT)) {
        ASTNode *node = ast_new(p->arena, AST_DEFAULT_STMT, loc);
        expect(p, TOK_COLON);
        node->default_stmt.stmt = parse_statement(p);
        return node;
    }

    /* Return */
    if (match(p, TOK_KW_RETURN)) {
        ASTNode *node = ast_new(p->arena, AST_RETURN_STMT, loc);
        if (check(p, TOK_SEMICOLON)) {
            node->return_stmt.expr = NULL;
        } else {
            node->return_stmt.expr = parse_expression(p);
        }
        expect(p, TOK_SEMICOLON);
        return node;
    }

    /* Break */
    if (match(p, TOK_KW_BREAK)) {
        ASTNode *node = ast_new(p->arena, AST_BREAK_STMT, loc);
        expect(p, TOK_SEMICOLON);
        return node;
    }

    /* Continue */
    if (match(p, TOK_KW_CONTINUE)) {
        ASTNode *node = ast_new(p->arena, AST_CONTINUE_STMT, loc);
        expect(p, TOK_SEMICOLON);
        return node;
    }

    /* Goto */
    if (match(p, TOK_KW_GOTO)) {
        ASTNode *node = ast_new(p->arena, AST_GOTO_STMT, loc);
        Token label = expect(p, TOK_IDENT);
        node->goto_stmt.label = label.text;
        expect(p, TOK_SEMICOLON);
        return node;
    }

    /* Label: identifier followed by ':' */
    if (check(p, TOK_IDENT)) {
        Token id = p->tok;
        /* Peek ahead for ':' */
        Token next = lexer_peek(p->lex);
        if (next.kind == TOK_COLON) {
            advance(p); /* consume identifier */
            advance(p); /* consume ':' */
            ASTNode *node = ast_new(p->arena, AST_LABEL_STMT, loc);
            node->label_stmt.label = id.text;
            node->label_stmt.stmt = parse_statement(p);
            return node;
        }
    }

    /* Null statement */
    if (match(p, TOK_SEMICOLON)) {
        return ast_new(p->arena, AST_NULL_STMT, loc);
    }

    /* Expression statement */
    ASTNode *node = ast_new(p->arena, AST_EXPR_STMT, loc);
    node->expr_stmt.expr = parse_expression(p);
    expect(p, TOK_SEMICOLON);
    return node;
}
