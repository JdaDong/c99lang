/*
 * c99lang - Parser (main entry point)
 */
#include "parser/parser.h"
#include <string.h>

/* ---- Helpers ---- */

static void advance(Parser *p) {
    p->tok = lexer_next(p->lex);
}

static Token peek(Parser *p) {
    return p->tok;
}

static bool check(Parser *p, TokenKind kind) {
    return p->tok.kind == kind;
}

static bool match(Parser *p, TokenKind kind) {
    if (p->tok.kind == kind) {
        advance(p);
        return true;
    }
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

/* ---- Init ---- */

void parser_init(Parser *p, Lexer *lex, Arena *arena,
                 Strtab *strtab, ErrorCtx *err) {
    p->lex = lex;
    p->arena = arena;
    p->strtab = strtab;
    p->err = err;
    vec_init(&p->typedef_names);
    advance(p); /* prime the first token */
}

/* ---- Type name checking ---- */

bool parser_is_typename(Parser *p) {
    TokenKind k = p->tok.kind;

    /* Type specifiers */
    if (k == TOK_KW_VOID || k == TOK_KW_CHAR || k == TOK_KW_SHORT ||
        k == TOK_KW_INT || k == TOK_KW_LONG || k == TOK_KW_FLOAT ||
        k == TOK_KW_DOUBLE || k == TOK_KW_SIGNED || k == TOK_KW_UNSIGNED ||
        k == TOK_KW__BOOL || k == TOK_KW__COMPLEX || k == TOK_KW__IMAGINARY ||
        k == TOK_KW_STRUCT || k == TOK_KW_UNION || k == TOK_KW_ENUM)
        return true;

    /* Type qualifiers */
    if (k == TOK_KW_CONST || k == TOK_KW_VOLATILE || k == TOK_KW_RESTRICT)
        return true;

    /* Storage class specifiers */
    if (k == TOK_KW_TYPEDEF || k == TOK_KW_EXTERN || k == TOK_KW_STATIC ||
        k == TOK_KW_AUTO || k == TOK_KW_REGISTER)
        return true;

    /* Function specifiers */
    if (k == TOK_KW_INLINE) return true;

    /* Typedef names */
    if (k == TOK_IDENT) {
        for (size_t i = 0; i < p->typedef_names.size; i++) {
            if (strcmp((const char *)vec_get(&p->typedef_names, i),
                       p->tok.text) == 0)
                return true;
        }
    }

    return false;
}

/* ---- Disambiguation: is it a declaration or an expression statement? ---- */

static bool is_declaration(Parser *p) {
    return parser_is_typename(p);
}

/* ---- Top-level parsing ---- */

static ASTNode *parse_external_declaration(Parser *p) {
    if (check(p, TOK_EOF)) return NULL;

    if (!is_declaration(p)) {
        diag_error(p->err, p->tok.loc,
                   "expected declaration, got '%s'", token_kind_str(p->tok.kind));
        advance(p);
        return NULL;
    }

    return parse_declaration(p);
}

ASTNode *parser_parse(Parser *p) {
    ASTNode *tu = ast_new(p->arena, AST_TRANSLATION_UNIT, p->tok.loc);
    vec_init(&tu->tu.decls);

    while (!check(p, TOK_EOF)) {
        ASTNode *decl = parse_external_declaration(p);
        if (decl) {
            vec_push(&tu->tu.decls, decl);
        }
    }

    return tu;
}
