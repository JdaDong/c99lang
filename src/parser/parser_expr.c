/*
 * c99lang - Parser: Expression parsing
 * Operator precedence climbing / Pratt-style parsing
 */
#include "parser/parser.h"
#include <string.h>

/* Helpers */
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

/* ---- Primary expression ---- */

static ASTNode *parse_primary(Parser *p) {
    SrcLoc loc = p->tok.loc;

    /* Integer literal */
    if (check(p, TOK_INT_LIT)) {
        ASTNode *node = ast_new(p->arena, AST_INT_LIT, loc);
        node->int_lit.value = p->tok.int_lit.value;
        node->int_lit.suffix = p->tok.int_lit.suffix;
        advance(p);
        return node;
    }

    /* Float literal */
    if (check(p, TOK_FLOAT_LIT)) {
        ASTNode *node = ast_new(p->arena, AST_FLOAT_LIT, loc);
        node->float_lit.value = p->tok.float_lit.value;
        node->float_lit.suffix = p->tok.float_lit.suffix;
        advance(p);
        return node;
    }

    /* Character literal */
    if (check(p, TOK_CHAR_LIT)) {
        ASTNode *node = ast_new(p->arena, AST_CHAR_LIT, loc);
        /* Simple: use first char after quote */
        if (p->tok.text_len >= 2) {
            if (p->tok.text[1] == '\\' && p->tok.text_len >= 3) {
                switch (p->tok.text[2]) {
                case 'n':  node->char_lit.value = '\n'; break;
                case 't':  node->char_lit.value = '\t'; break;
                case '0':  node->char_lit.value = '\0'; break;
                case '\\': node->char_lit.value = '\\'; break;
                case '\'': node->char_lit.value = '\''; break;
                default:   node->char_lit.value = p->tok.text[2]; break;
                }
            } else {
                node->char_lit.value = p->tok.text[1];
            }
        }
        advance(p);
        return node;
    }

    /* String literal */
    if (check(p, TOK_STRING_LIT)) {
        ASTNode *node = ast_new(p->arena, AST_STRING_LIT, loc);
        /* Store the raw text (with quotes) */
        node->string_lit.value = p->tok.text;
        node->string_lit.length = p->tok.text_len;
        advance(p);

        /* Concatenate adjacent string literals (C99) */
        while (check(p, TOK_STRING_LIT)) {
            /* TODO: proper concatenation */
            advance(p);
        }
        return node;
    }

    /* Identifier */
    if (check(p, TOK_IDENT)) {
        ASTNode *node = ast_new(p->arena, AST_IDENT_EXPR, loc);
        node->ident.name = p->tok.text;
        advance(p);
        return node;
    }

    /* Parenthesized expression or cast or compound literal */
    if (check(p, TOK_LPAREN)) {
        advance(p);

        /* Cast expression: (type-name) expr */
        if (parser_is_typename(p)) {
            StorageClass sc = SC_NONE;
            TypeQual tq = TQ_NONE;
            FuncSpec fs = FS_NONE;
            CType *cast_type = parse_type_specifier(p, &sc, &tq, &fs);
            cast_type = parse_abstract_declarator(p, cast_type);
            expect(p, TOK_RPAREN);

            /* C99 compound literal: (type){...} */
            if (check(p, TOK_LBRACE)) {
                ASTNode *node = ast_new(p->arena, AST_COMPOUND_LIT, loc);
                node->init_list.lit_type = cast_type;
                vec_init(&node->init_list.items);

                advance(p); /* skip { */
                while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
                    /* C99 designated initializer */
                    if (check(p, TOK_DOT) || check(p, TOK_LBRACKET)) {
                        ASTNode *desig = ast_new(p->arena, AST_DESIGNATOR, p->tok.loc);
                        desig->designator.index = NULL;
                        desig->designator.field = NULL;

                        if (match(p, TOK_LBRACKET)) {
                            desig->designator.index = parse_constant_expression(p);
                            expect(p, TOK_RBRACKET);
                        } else if (match(p, TOK_DOT)) {
                            Token fname = expect(p, TOK_IDENT);
                            desig->designator.field = fname.text;
                        }
                        expect(p, TOK_ASSIGN);
                        desig->designator.init = parse_assignment_expression(p);
                        vec_push(&node->init_list.items, desig);
                    } else {
                        ASTNode *item = parse_assignment_expression(p);
                        vec_push(&node->init_list.items, item);
                    }
                    if (!match(p, TOK_COMMA)) break;
                }
                expect(p, TOK_RBRACE);
                return node;
            }

            /* Regular cast */
            ASTNode *node = ast_new(p->arena, AST_CAST_EXPR, loc);
            node->cast.cast_type = cast_type;
            node->cast.operand = parse_expression(p);
            return node;
        }

        /* Parenthesized expression */
        ASTNode *expr = parse_expression(p);
        expect(p, TOK_RPAREN);
        return expr;
    }

    /* Initializer list { ... } */
    if (check(p, TOK_LBRACE)) {
        ASTNode *node = ast_new(p->arena, AST_INIT_LIST, loc);
        node->init_list.lit_type = NULL;
        vec_init(&node->init_list.items);

        advance(p);
        while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
            /* C99 designated initializer */
            if (check(p, TOK_DOT) || check(p, TOK_LBRACKET)) {
                ASTNode *desig = ast_new(p->arena, AST_DESIGNATOR, p->tok.loc);
                desig->designator.index = NULL;
                desig->designator.field = NULL;

                if (match(p, TOK_LBRACKET)) {
                    desig->designator.index = parse_constant_expression(p);
                    expect(p, TOK_RBRACKET);
                } else if (match(p, TOK_DOT)) {
                    Token fname = expect(p, TOK_IDENT);
                    desig->designator.field = fname.text;
                }
                expect(p, TOK_ASSIGN);
                desig->designator.init = parse_assignment_expression(p);
                vec_push(&node->init_list.items, desig);
            } else {
                vec_push(&node->init_list.items, parse_assignment_expression(p));
            }
            if (!match(p, TOK_COMMA)) break;
        }
        expect(p, TOK_RBRACE);
        return node;
    }

    diag_error(p->err, loc, "expected expression, got '%s'",
               token_kind_str(p->tok.kind));
    advance(p);
    return ast_new(p->arena, AST_INT_LIT, loc); /* error recovery */
}

/* ---- Postfix expression ---- */

static ASTNode *parse_postfix(Parser *p) {
    ASTNode *node = parse_primary(p);

    while (true) {
        SrcLoc loc = p->tok.loc;

        /* Function call: expr(...) */
        if (match(p, TOK_LPAREN)) {
            ASTNode *call = ast_new(p->arena, AST_CALL_EXPR, loc);
            call->call.callee = node;
            vec_init(&call->call.args);

            if (!check(p, TOK_RPAREN)) {
                vec_push(&call->call.args, parse_assignment_expression(p));
                while (match(p, TOK_COMMA)) {
                    vec_push(&call->call.args, parse_assignment_expression(p));
                }
            }
            expect(p, TOK_RPAREN);
            node = call;
            continue;
        }

        /* Array subscript: expr[expr] */
        if (match(p, TOK_LBRACKET)) {
            ASTNode *idx = ast_new(p->arena, AST_INDEX_EXPR, loc);
            idx->index_expr.array = node;
            idx->index_expr.index = parse_expression(p);
            expect(p, TOK_RBRACKET);
            node = idx;
            continue;
        }

        /* Member access: expr.member or expr->member */
        if (check(p, TOK_DOT) || check(p, TOK_ARROW)) {
            bool is_arrow = check(p, TOK_ARROW);
            advance(p);
            ASTNode *mem = ast_new(p->arena, AST_MEMBER_EXPR, loc);
            mem->member.object = node;
            mem->member.is_arrow = is_arrow;
            Token field = expect(p, TOK_IDENT);
            mem->member.member = field.text;
            node = mem;
            continue;
        }

        /* Postfix ++ / -- */
        if (check(p, TOK_PLUSPLUS)) {
            advance(p);
            ASTNode *post = ast_new(p->arena, AST_POSTFIX_EXPR, loc);
            post->postfix.op = POSTFIX_INC;
            post->postfix.operand = node;
            node = post;
            continue;
        }
        if (check(p, TOK_MINUSMINUS)) {
            advance(p);
            ASTNode *post = ast_new(p->arena, AST_POSTFIX_EXPR, loc);
            post->postfix.op = POSTFIX_DEC;
            post->postfix.operand = node;
            node = post;
            continue;
        }

        break;
    }

    return node;
}

/* ---- Unary expression ---- */

static ASTNode *parse_unary(Parser *p) {
    SrcLoc loc = p->tok.loc;

    /* sizeof */
    if (match(p, TOK_KW_SIZEOF)) {
        ASTNode *node = ast_new(p->arena, AST_SIZEOF_EXPR, loc);
        if (check(p, TOK_LPAREN)) {
            Token next = lexer_peek(p->lex);
            /* Try to detect sizeof(type) vs sizeof(expr) */
            advance(p); /* skip ( */
            if (parser_is_typename(p)) {
                StorageClass sc; TypeQual tq; FuncSpec fs;
                CType *ty = parse_type_specifier(p, &sc, &tq, &fs);
                ty = parse_abstract_declarator(p, ty);
                expect(p, TOK_RPAREN);
                node->sizeof_expr.is_type = true;
                node->sizeof_expr.sizeof_type = ty;
                node->sizeof_expr.sizeof_expr = NULL;
                return node;
            } else {
                ASTNode *expr = parse_expression(p);
                expect(p, TOK_RPAREN);
                node->sizeof_expr.is_type = false;
                node->sizeof_expr.sizeof_type = NULL;
                node->sizeof_expr.sizeof_expr = expr;
                return node;
            }
        }
        node->sizeof_expr.is_type = false;
        node->sizeof_expr.sizeof_type = NULL;
        node->sizeof_expr.sizeof_expr = parse_unary(p);
        return node;
    }

    /* Prefix ++ / -- */
    if (match(p, TOK_PLUSPLUS)) {
        ASTNode *node = ast_new(p->arena, AST_UNARY_EXPR, loc);
        node->unary.op = UNARY_PRE_INC;
        node->unary.operand = parse_unary(p);
        return node;
    }
    if (match(p, TOK_MINUSMINUS)) {
        ASTNode *node = ast_new(p->arena, AST_UNARY_EXPR, loc);
        node->unary.op = UNARY_PRE_DEC;
        node->unary.operand = parse_unary(p);
        return node;
    }

    /* Unary operators: & * + - ~ ! */
    if (match(p, TOK_AMP)) {
        ASTNode *node = ast_new(p->arena, AST_UNARY_EXPR, loc);
        node->unary.op = UNARY_ADDR;
        node->unary.operand = parse_unary(p); /* C99: cast-expression */
        return node;
    }
    if (match(p, TOK_STAR)) {
        ASTNode *node = ast_new(p->arena, AST_UNARY_EXPR, loc);
        node->unary.op = UNARY_DEREF;
        node->unary.operand = parse_unary(p);
        return node;
    }
    if (match(p, TOK_PLUS)) {
        ASTNode *node = ast_new(p->arena, AST_UNARY_EXPR, loc);
        node->unary.op = UNARY_POS;
        node->unary.operand = parse_unary(p);
        return node;
    }
    if (match(p, TOK_MINUS)) {
        ASTNode *node = ast_new(p->arena, AST_UNARY_EXPR, loc);
        node->unary.op = UNARY_NEG;
        node->unary.operand = parse_unary(p);
        return node;
    }
    if (match(p, TOK_TILDE)) {
        ASTNode *node = ast_new(p->arena, AST_UNARY_EXPR, loc);
        node->unary.op = UNARY_BITNOT;
        node->unary.operand = parse_unary(p);
        return node;
    }
    if (match(p, TOK_BANG)) {
        ASTNode *node = ast_new(p->arena, AST_UNARY_EXPR, loc);
        node->unary.op = UNARY_NOT;
        node->unary.operand = parse_unary(p);
        return node;
    }

    return parse_postfix(p);
}

/* ---- Binary expression (precedence climbing) ---- */

static int get_precedence(TokenKind kind) {
    switch (kind) {
    case TOK_STAR: case TOK_SLASH: case TOK_PERCENT: return 13;
    case TOK_PLUS: case TOK_MINUS:                    return 12;
    case TOK_LSHIFT: case TOK_RSHIFT:                 return 11;
    case TOK_LT: case TOK_GT: case TOK_LE: case TOK_GE: return 10;
    case TOK_EQ: case TOK_NE:                         return 9;
    case TOK_AMP:                                      return 8;
    case TOK_CARET:                                    return 7;
    case TOK_PIPE:                                     return 6;
    case TOK_AMPAMP:                                   return 5;
    case TOK_PIPEPIPE:                                 return 4;
    default: return -1;
    }
}

static BinOp token_to_binop(TokenKind kind) {
    switch (kind) {
    case TOK_STAR:     return BIN_MUL;
    case TOK_SLASH:    return BIN_DIV;
    case TOK_PERCENT:  return BIN_MOD;
    case TOK_PLUS:     return BIN_ADD;
    case TOK_MINUS:    return BIN_SUB;
    case TOK_LSHIFT:   return BIN_LSHIFT;
    case TOK_RSHIFT:   return BIN_RSHIFT;
    case TOK_LT:      return BIN_LT;
    case TOK_GT:      return BIN_GT;
    case TOK_LE:      return BIN_LE;
    case TOK_GE:      return BIN_GE;
    case TOK_EQ:      return BIN_EQ;
    case TOK_NE:      return BIN_NE;
    case TOK_AMP:      return BIN_BIT_AND;
    case TOK_CARET:    return BIN_BIT_XOR;
    case TOK_PIPE:     return BIN_BIT_OR;
    case TOK_AMPAMP:   return BIN_LOG_AND;
    case TOK_PIPEPIPE: return BIN_LOG_OR;
    default: return BIN_ADD; /* shouldn't happen */
    }
}

static ASTNode *parse_binary(Parser *p, int min_prec) {
    ASTNode *lhs = parse_unary(p);

    while (true) {
        int prec = get_precedence(p->tok.kind);
        if (prec < min_prec) break;

        SrcLoc loc = p->tok.loc;
        BinOp op = token_to_binop(p->tok.kind);
        advance(p);

        ASTNode *rhs = parse_binary(p, prec + 1);

        ASTNode *bin = ast_new(p->arena, AST_BINARY_EXPR, loc);
        bin->binary.op = op;
        bin->binary.lhs = lhs;
        bin->binary.rhs = rhs;
        lhs = bin;
    }

    return lhs;
}

/* ---- Conditional expression: a ? b : c ---- */

ASTNode *parse_conditional_expression(Parser *p) {
    ASTNode *cond = parse_binary(p, 0);

    if (match(p, TOK_QUESTION)) {
        SrcLoc loc = p->tok.loc;
        ASTNode *then_expr = parse_expression(p);
        expect(p, TOK_COLON);
        ASTNode *else_expr = parse_conditional_expression(p);

        ASTNode *ternary = ast_new(p->arena, AST_TERNARY_EXPR, loc);
        ternary->ternary.cond = cond;
        ternary->ternary.then_expr = then_expr;
        ternary->ternary.else_expr = else_expr;
        return ternary;
    }

    return cond;
}

/* ---- Assignment expression ---- */

static AssignOp token_to_assignop(TokenKind kind) {
    switch (kind) {
    case TOK_ASSIGN:         return ASSIGN_EQ;
    case TOK_STAR_ASSIGN:    return ASSIGN_MUL;
    case TOK_SLASH_ASSIGN:   return ASSIGN_DIV;
    case TOK_PERCENT_ASSIGN: return ASSIGN_MOD;
    case TOK_PLUS_ASSIGN:    return ASSIGN_ADD;
    case TOK_MINUS_ASSIGN:   return ASSIGN_SUB;
    case TOK_LSHIFT_ASSIGN:  return ASSIGN_LSHIFT;
    case TOK_RSHIFT_ASSIGN:  return ASSIGN_RSHIFT;
    case TOK_AMP_ASSIGN:     return ASSIGN_AND;
    case TOK_CARET_ASSIGN:   return ASSIGN_XOR;
    case TOK_PIPE_ASSIGN:    return ASSIGN_OR;
    default: return ASSIGN_EQ;
    }
}

static bool is_assignment_op(TokenKind kind) {
    return kind == TOK_ASSIGN || kind == TOK_STAR_ASSIGN ||
           kind == TOK_SLASH_ASSIGN || kind == TOK_PERCENT_ASSIGN ||
           kind == TOK_PLUS_ASSIGN || kind == TOK_MINUS_ASSIGN ||
           kind == TOK_LSHIFT_ASSIGN || kind == TOK_RSHIFT_ASSIGN ||
           kind == TOK_AMP_ASSIGN || kind == TOK_CARET_ASSIGN ||
           kind == TOK_PIPE_ASSIGN;
}

ASTNode *parse_assignment_expression(Parser *p) {
    ASTNode *lhs = parse_conditional_expression(p);

    if (is_assignment_op(p->tok.kind)) {
        SrcLoc loc = p->tok.loc;
        AssignOp op = token_to_assignop(p->tok.kind);
        advance(p);
        ASTNode *rhs = parse_assignment_expression(p); /* right-associative */

        ASTNode *node = ast_new(p->arena, AST_ASSIGN_EXPR, loc);
        node->assign.op = op;
        node->assign.lhs = lhs;
        node->assign.rhs = rhs;
        return node;
    }

    return lhs;
}

/* ---- Comma expression ---- */

ASTNode *parse_expression(Parser *p) {
    ASTNode *lhs = parse_assignment_expression(p);

    while (match(p, TOK_COMMA)) {
        SrcLoc loc = p->tok.loc;
        ASTNode *rhs = parse_assignment_expression(p);
        ASTNode *comma = ast_new(p->arena, AST_COMMA_EXPR, loc);
        comma->comma.lhs = lhs;
        comma->comma.rhs = rhs;
        lhs = comma;
    }

    return lhs;
}

/* ---- Constant expression ---- */

ASTNode *parse_constant_expression(Parser *p) {
    return parse_conditional_expression(p);
}
