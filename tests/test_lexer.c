/*
 * c99lang - Lexer test
 */
#include <stdio.h>
#include <string.h>
#include "lexer/lexer.h"
#include "util/strtab.h"
#include "util/error.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_EQ(a, b, msg) do { \
    if ((a) == (b)) { tests_passed++; } \
    else { tests_failed++; \
           printf("FAIL: %s (expected %d, got %d) at %s:%d\n", \
                  msg, (int)(b), (int)(a), __FILE__, __LINE__); } \
} while(0)

static void test_basic_tokens(void) {
    const char *src = "int main(void) { return 42; }";
    Strtab st;
    ErrorCtx err;
    strtab_init(&st);
    error_ctx_init(&err);

    Lexer lex;
    lexer_init(&lex, src, strlen(src), "test.c", &st, &err);

    ASSERT_EQ(lexer_next(&lex).kind, TOK_KW_INT, "int keyword");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_IDENT, "main identifier");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_LPAREN, "(");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_KW_VOID, "void keyword");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_RPAREN, ")");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_LBRACE, "{");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_KW_RETURN, "return keyword");

    Token num = lexer_next(&lex);
    ASSERT_EQ(num.kind, TOK_INT_LIT, "integer literal");
    ASSERT_EQ((int)num.int_lit.value, 42, "literal value 42");

    ASSERT_EQ(lexer_next(&lex).kind, TOK_SEMICOLON, ";");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_RBRACE, "}");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_EOF, "EOF");

    strtab_free(&st);
}

static void test_operators(void) {
    const char *src = "++ -- -> << >> <= >= == != && || += -= *= /= %= <<= >>= &= ^= |= ...";
    Strtab st;
    ErrorCtx err;
    strtab_init(&st);
    error_ctx_init(&err);

    Lexer lex;
    lexer_init(&lex, src, strlen(src), "test.c", &st, &err);

    ASSERT_EQ(lexer_next(&lex).kind, TOK_PLUSPLUS, "++");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_MINUSMINUS, "--");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_ARROW, "->");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_LSHIFT, "<<");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_RSHIFT, ">>");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_LE, "<=");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_GE, ">=");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_EQ, "==");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_NE, "!=");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_AMPAMP, "&&");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_PIPEPIPE, "||");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_PLUS_ASSIGN, "+=");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_MINUS_ASSIGN, "-=");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_STAR_ASSIGN, "*=");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_SLASH_ASSIGN, "/=");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_PERCENT_ASSIGN, "%=");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_LSHIFT_ASSIGN, "<<=");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_RSHIFT_ASSIGN, ">>=");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_AMP_ASSIGN, "&=");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_CARET_ASSIGN, "^=");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_PIPE_ASSIGN, "|=");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_ELLIPSIS, "...");

    strtab_free(&st);
}

static void test_numbers(void) {
    const char *src = "42 0xFF 077 3.14 1e10 0x1.0p10 42u 42L 42ULL 3.14f 3.14L";
    Strtab st;
    ErrorCtx err;
    strtab_init(&st);
    error_ctx_init(&err);

    Lexer lex;
    lexer_init(&lex, src, strlen(src), "test.c", &st, &err);

    Token t;
    t = lexer_next(&lex); ASSERT_EQ(t.kind, TOK_INT_LIT, "42");
    ASSERT_EQ((int)t.int_lit.value, 42, "value 42");

    t = lexer_next(&lex); ASSERT_EQ(t.kind, TOK_INT_LIT, "0xFF");
    ASSERT_EQ((int)t.int_lit.value, 255, "value 0xFF");

    t = lexer_next(&lex); ASSERT_EQ(t.kind, TOK_INT_LIT, "077");
    ASSERT_EQ((int)t.int_lit.value, 63, "value 077");

    t = lexer_next(&lex); ASSERT_EQ(t.kind, TOK_FLOAT_LIT, "3.14");
    t = lexer_next(&lex); ASSERT_EQ(t.kind, TOK_FLOAT_LIT, "1e10");
    t = lexer_next(&lex); ASSERT_EQ(t.kind, TOK_FLOAT_LIT, "0x1.0p10");

    t = lexer_next(&lex); ASSERT_EQ(t.kind, TOK_INT_LIT, "42u");
    ASSERT_EQ(t.int_lit.suffix, INT_SUFFIX_U, "unsigned suffix");

    t = lexer_next(&lex); ASSERT_EQ(t.kind, TOK_INT_LIT, "42L");
    ASSERT_EQ(t.int_lit.suffix, INT_SUFFIX_L, "long suffix");

    t = lexer_next(&lex); ASSERT_EQ(t.kind, TOK_INT_LIT, "42ULL");
    ASSERT_EQ(t.int_lit.suffix, INT_SUFFIX_ULL, "unsigned long long suffix");

    t = lexer_next(&lex); ASSERT_EQ(t.kind, TOK_FLOAT_LIT, "3.14f");
    ASSERT_EQ(t.float_lit.suffix, FLOAT_SUFFIX_F, "float suffix");

    t = lexer_next(&lex); ASSERT_EQ(t.kind, TOK_FLOAT_LIT, "3.14L");
    ASSERT_EQ(t.float_lit.suffix, FLOAT_SUFFIX_L, "long double suffix");

    strtab_free(&st);
}

static void test_c99_keywords(void) {
    const char *src = "inline restrict _Bool _Complex _Imaginary";
    Strtab st;
    ErrorCtx err;
    strtab_init(&st);
    error_ctx_init(&err);

    Lexer lex;
    lexer_init(&lex, src, strlen(src), "test.c", &st, &err);

    ASSERT_EQ(lexer_next(&lex).kind, TOK_KW_INLINE, "inline");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_KW_RESTRICT, "restrict");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_KW__BOOL, "_Bool");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_KW__COMPLEX, "_Complex");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_KW__IMAGINARY, "_Imaginary");

    strtab_free(&st);
}

static void test_comments(void) {
    const char *src = "a // line comment\nb /* block */ c";
    Strtab st;
    ErrorCtx err;
    strtab_init(&st);
    error_ctx_init(&err);

    Lexer lex;
    lexer_init(&lex, src, strlen(src), "test.c", &st, &err);

    ASSERT_EQ(lexer_next(&lex).kind, TOK_IDENT, "a before line comment");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_IDENT, "b after line comment");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_IDENT, "c after block comment");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_EOF, "EOF");

    strtab_free(&st);
}

int main(void) {
    printf("=== Lexer Tests ===\n");

    test_basic_tokens();
    test_operators();
    test_numbers();
    test_c99_keywords();
    test_comments();

    printf("\n%d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
