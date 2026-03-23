/*
 * c99lang - Extended Lexer tests
 * Covers: strings, chars, edge cases, error recovery, line tracking
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

#define ASSERT_TRUE(cond, msg) do { \
    if (cond) { tests_passed++; } \
    else { tests_failed++; \
           printf("FAIL: %s at %s:%d\n", msg, __FILE__, __LINE__); } \
} while(0)

#define ASSERT_STR_EQ(a, b, msg) do { \
    if ((a) && (b) && strcmp((a), (b)) == 0) { tests_passed++; } \
    else { tests_failed++; \
           printf("FAIL: %s (expected \"%s\", got \"%s\") at %s:%d\n", \
                  msg, (b), (a) ? (a) : "(null)", __FILE__, __LINE__); } \
} while(0)

/* Helper to create a lexer from a string */
static void lex_from(Lexer *lex, const char *src, Strtab *st, ErrorCtx *err) {
    lexer_init(lex, src, strlen(src), "test.c", st, err);
}

static void test_string_literal_simple(void) {
    Strtab st; ErrorCtx err;
    strtab_init(&st); error_ctx_init(&err);
    Lexer lex;
    lex_from(&lex, "\"hello world\"", &st, &err);

    Token t = lexer_next(&lex);
    ASSERT_EQ(t.kind, TOK_STRING_LIT, "string literal kind");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_EOF, "string: EOF");

    strtab_free(&st);
}

static void test_string_literal_escape(void) {
    Strtab st; ErrorCtx err;
    strtab_init(&st); error_ctx_init(&err);
    Lexer lex;
    lex_from(&lex, "\"hello\\nworld\" \"tab\\there\"", &st, &err);

    Token t1 = lexer_next(&lex);
    ASSERT_EQ(t1.kind, TOK_STRING_LIT, "escape string 1");
    Token t2 = lexer_next(&lex);
    ASSERT_EQ(t2.kind, TOK_STRING_LIT, "escape string 2");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_EOF, "escape strings: EOF");

    strtab_free(&st);
}

static void test_string_literal_empty(void) {
    Strtab st; ErrorCtx err;
    strtab_init(&st); error_ctx_init(&err);
    Lexer lex;
    lex_from(&lex, "\"\"", &st, &err);

    Token t = lexer_next(&lex);
    ASSERT_EQ(t.kind, TOK_STRING_LIT, "empty string literal");

    strtab_free(&st);
}

static void test_char_literal_simple(void) {
    Strtab st; ErrorCtx err;
    strtab_init(&st); error_ctx_init(&err);
    Lexer lex;
    lex_from(&lex, "'a' 'Z' '0'", &st, &err);

    Token t1 = lexer_next(&lex);
    ASSERT_EQ(t1.kind, TOK_CHAR_LIT, "char 'a'");

    Token t2 = lexer_next(&lex);
    ASSERT_EQ(t2.kind, TOK_CHAR_LIT, "char 'Z'");

    Token t3 = lexer_next(&lex);
    ASSERT_EQ(t3.kind, TOK_CHAR_LIT, "char '0'");

    strtab_free(&st);
}

static void test_char_literal_escape(void) {
    Strtab st; ErrorCtx err;
    strtab_init(&st); error_ctx_init(&err);
    Lexer lex;
    lex_from(&lex, "'\\n' '\\t' '\\0' '\\\\' '\\''", &st, &err);

    ASSERT_EQ(lexer_next(&lex).kind, TOK_CHAR_LIT, "char \\n");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_CHAR_LIT, "char \\t");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_CHAR_LIT, "char \\0");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_CHAR_LIT, "char \\\\");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_CHAR_LIT, "char \\'");

    strtab_free(&st);
}

static void test_all_keywords(void) {
    Strtab st; ErrorCtx err;
    strtab_init(&st); error_ctx_init(&err);
    Lexer lex;

    const char *src =
        "auto break case char const continue default do double else "
        "enum extern float for goto if inline int long register "
        "restrict return short signed sizeof static struct switch "
        "typedef union unsigned void volatile while _Bool _Complex _Imaginary";
    lex_from(&lex, src, &st, &err);

    TokenKind expected[] = {
        TOK_KW_AUTO, TOK_KW_BREAK, TOK_KW_CASE, TOK_KW_CHAR, TOK_KW_CONST,
        TOK_KW_CONTINUE, TOK_KW_DEFAULT, TOK_KW_DO, TOK_KW_DOUBLE, TOK_KW_ELSE,
        TOK_KW_ENUM, TOK_KW_EXTERN, TOK_KW_FLOAT, TOK_KW_FOR, TOK_KW_GOTO,
        TOK_KW_IF, TOK_KW_INLINE, TOK_KW_INT, TOK_KW_LONG, TOK_KW_REGISTER,
        TOK_KW_RESTRICT, TOK_KW_RETURN, TOK_KW_SHORT, TOK_KW_SIGNED,
        TOK_KW_SIZEOF, TOK_KW_STATIC, TOK_KW_STRUCT, TOK_KW_SWITCH,
        TOK_KW_TYPEDEF, TOK_KW_UNION, TOK_KW_UNSIGNED, TOK_KW_VOID,
        TOK_KW_VOLATILE, TOK_KW_WHILE, TOK_KW__BOOL, TOK_KW__COMPLEX,
        TOK_KW__IMAGINARY
    };
    int n = sizeof(expected) / sizeof(expected[0]);

    for (int i = 0; i < n; i++) {
        Token t = lexer_next(&lex);
        ASSERT_EQ(t.kind, expected[i], "keyword token kind");
    }
    ASSERT_EQ(lexer_next(&lex).kind, TOK_EOF, "all keywords: EOF");

    strtab_free(&st);
}

static void test_single_char_operators(void) {
    Strtab st; ErrorCtx err;
    strtab_init(&st); error_ctx_init(&err);
    Lexer lex;
    lex_from(&lex, "( ) [ ] { } . , ; : ? ~ + - * / % & | ^ ! < > =", &st, &err);

    TokenKind expected[] = {
        TOK_LPAREN, TOK_RPAREN, TOK_LBRACKET, TOK_RBRACKET,
        TOK_LBRACE, TOK_RBRACE, TOK_DOT, TOK_COMMA,
        TOK_SEMICOLON, TOK_COLON, TOK_QUESTION, TOK_TILDE,
        TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH,
        TOK_PERCENT, TOK_AMP, TOK_PIPE, TOK_CARET,
        TOK_BANG, TOK_LT, TOK_GT, TOK_ASSIGN
    };
    int n = sizeof(expected) / sizeof(expected[0]);

    for (int i = 0; i < n; i++) {
        Token t = lexer_next(&lex);
        ASSERT_EQ(t.kind, expected[i], "single char operator");
    }

    strtab_free(&st);
}

static void test_hex_numbers(void) {
    Strtab st; ErrorCtx err;
    strtab_init(&st); error_ctx_init(&err);
    Lexer lex;
    lex_from(&lex, "0x0 0xFF 0xABCDEF 0X1234", &st, &err);

    Token t;
    t = lexer_next(&lex);
    ASSERT_EQ(t.kind, TOK_INT_LIT, "hex 0x0");
    ASSERT_EQ((int)t.int_lit.value, 0, "hex 0x0 value");

    t = lexer_next(&lex);
    ASSERT_EQ(t.kind, TOK_INT_LIT, "hex 0xFF");
    ASSERT_EQ((int)t.int_lit.value, 255, "hex 0xFF value");

    t = lexer_next(&lex);
    ASSERT_EQ(t.kind, TOK_INT_LIT, "hex 0xABCDEF");
    ASSERT_EQ((int)t.int_lit.value, 0xABCDEF, "hex 0xABCDEF value");

    t = lexer_next(&lex);
    ASSERT_EQ(t.kind, TOK_INT_LIT, "hex 0X1234");
    ASSERT_EQ((int)t.int_lit.value, 0x1234, "hex 0X1234 value");

    strtab_free(&st);
}

static void test_octal_numbers(void) {
    Strtab st; ErrorCtx err;
    strtab_init(&st); error_ctx_init(&err);
    Lexer lex;
    lex_from(&lex, "00 07 010 0177 0777", &st, &err);

    Token t;
    t = lexer_next(&lex);
    ASSERT_EQ(t.kind, TOK_INT_LIT, "octal 00");
    ASSERT_EQ((int)t.int_lit.value, 0, "octal 00 value");

    t = lexer_next(&lex);
    ASSERT_EQ(t.kind, TOK_INT_LIT, "octal 07");
    ASSERT_EQ((int)t.int_lit.value, 7, "octal 07 value");

    t = lexer_next(&lex);
    ASSERT_EQ(t.kind, TOK_INT_LIT, "octal 010");
    ASSERT_EQ((int)t.int_lit.value, 8, "octal 010 value");

    t = lexer_next(&lex);
    ASSERT_EQ(t.kind, TOK_INT_LIT, "octal 0177");
    ASSERT_EQ((int)t.int_lit.value, 127, "octal 0177 value");

    t = lexer_next(&lex);
    ASSERT_EQ(t.kind, TOK_INT_LIT, "octal 0777");
    ASSERT_EQ((int)t.int_lit.value, 511, "octal 0777 value");

    strtab_free(&st);
}

static void test_float_literals(void) {
    Strtab st; ErrorCtx err;
    strtab_init(&st); error_ctx_init(&err);
    Lexer lex;
    lex_from(&lex, "0.0 1.5 .5 3.14159 1e10 2.5e-3 1E+5", &st, &err);

    ASSERT_EQ(lexer_next(&lex).kind, TOK_FLOAT_LIT, "float 0.0");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_FLOAT_LIT, "float 1.5");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_FLOAT_LIT, "float .5");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_FLOAT_LIT, "float 3.14159");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_FLOAT_LIT, "float 1e10");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_FLOAT_LIT, "float 2.5e-3");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_FLOAT_LIT, "float 1E+5");

    strtab_free(&st);
}

static void test_integer_suffixes(void) {
    Strtab st; ErrorCtx err;
    strtab_init(&st); error_ctx_init(&err);
    Lexer lex;
    lex_from(&lex, "42u 42U 42l 42L 42ll 42LL 42ul 42UL 42ull 42ULL 42lu 42LLU",
             &st, &err);

    Token t;
    t = lexer_next(&lex); ASSERT_EQ(t.kind, TOK_INT_LIT, "42u");
    ASSERT_EQ(t.int_lit.suffix, INT_SUFFIX_U, "42u suffix");

    t = lexer_next(&lex); ASSERT_EQ(t.kind, TOK_INT_LIT, "42U");
    ASSERT_EQ(t.int_lit.suffix, INT_SUFFIX_U, "42U suffix");

    t = lexer_next(&lex); ASSERT_EQ(t.kind, TOK_INT_LIT, "42l");
    ASSERT_EQ(t.int_lit.suffix, INT_SUFFIX_L, "42l suffix");

    t = lexer_next(&lex); ASSERT_EQ(t.kind, TOK_INT_LIT, "42L");
    ASSERT_EQ(t.int_lit.suffix, INT_SUFFIX_L, "42L suffix");

    t = lexer_next(&lex); ASSERT_EQ(t.kind, TOK_INT_LIT, "42ll");
    ASSERT_EQ(t.int_lit.suffix, INT_SUFFIX_LL, "42ll suffix");

    t = lexer_next(&lex); ASSERT_EQ(t.kind, TOK_INT_LIT, "42LL");
    ASSERT_EQ(t.int_lit.suffix, INT_SUFFIX_LL, "42LL suffix");

    t = lexer_next(&lex); ASSERT_EQ(t.kind, TOK_INT_LIT, "42ul");
    ASSERT_EQ(t.int_lit.suffix, INT_SUFFIX_UL, "42ul suffix");

    t = lexer_next(&lex); ASSERT_EQ(t.kind, TOK_INT_LIT, "42UL");
    ASSERT_EQ(t.int_lit.suffix, INT_SUFFIX_UL, "42UL suffix");

    t = lexer_next(&lex); ASSERT_EQ(t.kind, TOK_INT_LIT, "42ull");
    ASSERT_EQ(t.int_lit.suffix, INT_SUFFIX_ULL, "42ull suffix");

    t = lexer_next(&lex); ASSERT_EQ(t.kind, TOK_INT_LIT, "42ULL");
    ASSERT_EQ(t.int_lit.suffix, INT_SUFFIX_ULL, "42ULL suffix");

    /* 42lu and 42LLU also valid */
    t = lexer_next(&lex); ASSERT_EQ(t.kind, TOK_INT_LIT, "42lu");
    t = lexer_next(&lex); ASSERT_EQ(t.kind, TOK_INT_LIT, "42LLU");

    strtab_free(&st);
}

static void test_float_suffixes(void) {
    Strtab st; ErrorCtx err;
    strtab_init(&st); error_ctx_init(&err);
    Lexer lex;
    lex_from(&lex, "1.0 1.0f 1.0F 1.0l 1.0L", &st, &err);

    Token t;
    t = lexer_next(&lex); ASSERT_EQ(t.kind, TOK_FLOAT_LIT, "1.0 double");
    ASSERT_EQ(t.float_lit.suffix, FLOAT_SUFFIX_NONE, "1.0 no suffix");

    t = lexer_next(&lex); ASSERT_EQ(t.kind, TOK_FLOAT_LIT, "1.0f");
    ASSERT_EQ(t.float_lit.suffix, FLOAT_SUFFIX_F, "1.0f suffix F");

    t = lexer_next(&lex); ASSERT_EQ(t.kind, TOK_FLOAT_LIT, "1.0F");
    ASSERT_EQ(t.float_lit.suffix, FLOAT_SUFFIX_F, "1.0F suffix F");

    t = lexer_next(&lex); ASSERT_EQ(t.kind, TOK_FLOAT_LIT, "1.0l");
    ASSERT_EQ(t.float_lit.suffix, FLOAT_SUFFIX_L, "1.0l suffix L");

    t = lexer_next(&lex); ASSERT_EQ(t.kind, TOK_FLOAT_LIT, "1.0L");
    ASSERT_EQ(t.float_lit.suffix, FLOAT_SUFFIX_L, "1.0L suffix L");

    strtab_free(&st);
}

static void test_line_tracking(void) {
    Strtab st; ErrorCtx err;
    strtab_init(&st); error_ctx_init(&err);
    Lexer lex;
    lex_from(&lex, "int\n  x\n    =\n      42\n;", &st, &err);

    Token t;
    t = lexer_next(&lex); /* int */
    ASSERT_EQ(t.loc.line, 1, "int on line 1");

    t = lexer_next(&lex); /* x */
    ASSERT_EQ(t.loc.line, 2, "x on line 2");

    t = lexer_next(&lex); /* = */
    ASSERT_EQ(t.loc.line, 3, "= on line 3");

    t = lexer_next(&lex); /* 42 */
    ASSERT_EQ(t.loc.line, 4, "42 on line 4");

    t = lexer_next(&lex); /* ; */
    ASSERT_EQ(t.loc.line, 5, "; on line 5");

    strtab_free(&st);
}

static void test_multiline_comment(void) {
    Strtab st; ErrorCtx err;
    strtab_init(&st); error_ctx_init(&err);
    Lexer lex;
    lex_from(&lex, "a /* multi\n  line\n  comment */ b", &st, &err);

    Token t;
    t = lexer_next(&lex);
    ASSERT_EQ(t.kind, TOK_IDENT, "a before multiline comment");

    t = lexer_next(&lex);
    ASSERT_EQ(t.kind, TOK_IDENT, "b after multiline comment");

    ASSERT_EQ(lexer_next(&lex).kind, TOK_EOF, "multiline comment: EOF");

    strtab_free(&st);
}

static void test_adjacent_tokens(void) {
    Strtab st; ErrorCtx err;
    strtab_init(&st); error_ctx_init(&err);
    Lexer lex;
    lex_from(&lex, "a+b*c", &st, &err);

    ASSERT_EQ(lexer_next(&lex).kind, TOK_IDENT, "a");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_PLUS, "+");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_IDENT, "b");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_STAR, "*");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_IDENT, "c");

    strtab_free(&st);
}

static void test_identifier_names(void) {
    Strtab st; ErrorCtx err;
    strtab_init(&st); error_ctx_init(&err);
    Lexer lex;
    lex_from(&lex, "_foo __bar baz123 _123", &st, &err);

    Token t;
    t = lexer_next(&lex);
    ASSERT_EQ(t.kind, TOK_IDENT, "_foo is ident");

    t = lexer_next(&lex);
    ASSERT_EQ(t.kind, TOK_IDENT, "__bar is ident");

    t = lexer_next(&lex);
    ASSERT_EQ(t.kind, TOK_IDENT, "baz123 is ident");

    t = lexer_next(&lex);
    ASSERT_EQ(t.kind, TOK_IDENT, "_123 is ident");

    strtab_free(&st);
}

static void test_peek_and_match(void) {
    Strtab st; ErrorCtx err;
    strtab_init(&st); error_ctx_init(&err);
    Lexer lex;
    lex_from(&lex, "int x ;", &st, &err);

    /* Peek should not consume */
    Token peek1 = lexer_peek(&lex);
    ASSERT_EQ(peek1.kind, TOK_KW_INT, "peek returns int");

    Token peek2 = lexer_peek(&lex);
    ASSERT_EQ(peek2.kind, TOK_KW_INT, "second peek still returns int");

    /* Now consume */
    Token next = lexer_next(&lex);
    ASSERT_EQ(next.kind, TOK_KW_INT, "next after peek returns int");

    /* Match */
    ASSERT_TRUE(lexer_match(&lex, TOK_IDENT), "match IDENT succeeds");
    ASSERT_TRUE(!lexer_match(&lex, TOK_IDENT), "match IDENT fails (have ;)");

    strtab_free(&st);
}

static void test_eof_repeated(void) {
    Strtab st; ErrorCtx err;
    strtab_init(&st); error_ctx_init(&err);
    Lexer lex;
    lex_from(&lex, "", &st, &err);

    /* Multiple EOF reads should be safe */
    ASSERT_EQ(lexer_next(&lex).kind, TOK_EOF, "first EOF");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_EOF, "second EOF");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_EOF, "third EOF");

    strtab_free(&st);
}

static void test_hex_float(void) {
    Strtab st; ErrorCtx err;
    strtab_init(&st); error_ctx_init(&err);
    Lexer lex;
    lex_from(&lex, "0x1.0p10 0x1p0 0xA.Bp3", &st, &err);

    ASSERT_EQ(lexer_next(&lex).kind, TOK_FLOAT_LIT, "hex float 0x1.0p10");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_FLOAT_LIT, "hex float 0x1p0");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_FLOAT_LIT, "hex float 0xA.Bp3");

    strtab_free(&st);
}

static void test_zero_literal(void) {
    Strtab st; ErrorCtx err;
    strtab_init(&st); error_ctx_init(&err);
    Lexer lex;
    lex_from(&lex, "0", &st, &err);

    Token t = lexer_next(&lex);
    ASSERT_EQ(t.kind, TOK_INT_LIT, "zero is int literal");
    ASSERT_EQ((int)t.int_lit.value, 0, "zero value is 0");

    strtab_free(&st);
}

static void test_whitespace_only(void) {
    Strtab st; ErrorCtx err;
    strtab_init(&st); error_ctx_init(&err);
    Lexer lex;
    lex_from(&lex, "   \t\n  \n\t  ", &st, &err);

    ASSERT_EQ(lexer_next(&lex).kind, TOK_EOF, "whitespace only: EOF");

    strtab_free(&st);
}

static void test_complex_expression(void) {
    Strtab st; ErrorCtx err;
    strtab_init(&st); error_ctx_init(&err);
    Lexer lex;
    lex_from(&lex, "a->b.c[d](e, f) ? g : h", &st, &err);

    ASSERT_EQ(lexer_next(&lex).kind, TOK_IDENT, "a");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_ARROW, "->");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_IDENT, "b");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_DOT, ".");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_IDENT, "c");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_LBRACKET, "[");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_IDENT, "d");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_RBRACKET, "]");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_LPAREN, "(");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_IDENT, "e");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_COMMA, ",");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_IDENT, "f");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_RPAREN, ")");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_QUESTION, "?");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_IDENT, "g");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_COLON, ":");
    ASSERT_EQ(lexer_next(&lex).kind, TOK_IDENT, "h");

    strtab_free(&st);
}

int main(void) {
    printf("=== Extended Lexer Tests ===\n");

    test_string_literal_simple();
    test_string_literal_escape();
    test_string_literal_empty();
    test_char_literal_simple();
    test_char_literal_escape();
    test_all_keywords();
    test_single_char_operators();
    test_hex_numbers();
    test_octal_numbers();
    test_float_literals();
    test_integer_suffixes();
    test_float_suffixes();
    test_line_tracking();
    test_multiline_comment();
    test_adjacent_tokens();
    test_identifier_names();
    test_peek_and_match();
    test_eof_repeated();
    test_hex_float();
    test_zero_literal();
    test_whitespace_only();
    test_complex_expression();

    printf("\n%d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
