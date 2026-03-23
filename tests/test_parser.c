/*
 * c99lang - Parser test
 */
#include <stdio.h>
#include <string.h>
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "util/arena.h"
#include "util/strtab.h"
#include "util/error.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_TRUE(cond, msg) do { \
    if (cond) { tests_passed++; } \
    else { tests_failed++; \
           printf("FAIL: %s at %s:%d\n", msg, __FILE__, __LINE__); } \
} while(0)

static ASTNode *parse_string(const char *src, Arena *arena, Strtab *st,
                              ErrorCtx *err) {
    Lexer lex;
    lexer_init(&lex, src, strlen(src), "test.c", st, err);
    Parser parser;
    parser_init(&parser, &lex, arena, st, err);
    return parser_parse(&parser);
}

static void test_simple_function(void) {
    Arena arena;
    Strtab st;
    ErrorCtx err;
    arena_init(&arena);
    strtab_init(&st);
    error_ctx_init(&err);

    const char *src = "int main(void) { return 0; }";
    ASTNode *tu = parse_string(src, &arena, &st, &err);

    ASSERT_TRUE(tu != NULL, "parse returns non-null");
    ASSERT_TRUE(tu->kind == AST_TRANSLATION_UNIT, "translation unit");
    ASSERT_TRUE(tu->tu.decls.size == 1, "one declaration");

    ASTNode *func = (ASTNode *)vec_get(&tu->tu.decls, 0);
    ASSERT_TRUE(func->kind == AST_FUNC_DEF, "function definition");
    ASSERT_TRUE(strcmp(func->func_def.name, "main") == 0, "func name is main");

    ASSERT_TRUE(err.error_count == 0, "no parse errors");

    strtab_free(&st);
    arena_free(&arena);
}

static void test_variable_declaration(void) {
    Arena arena;
    Strtab st;
    ErrorCtx err;
    arena_init(&arena);
    strtab_init(&st);
    error_ctx_init(&err);

    const char *src = "int x = 42;";
    ASTNode *tu = parse_string(src, &arena, &st, &err);

    ASSERT_TRUE(tu != NULL, "parse returns non-null");
    ASSERT_TRUE(tu->tu.decls.size == 1, "one declaration");

    ASTNode *decl = (ASTNode *)vec_get(&tu->tu.decls, 0);
    ASSERT_TRUE(decl->kind == AST_VAR_DECL, "variable declaration");
    ASSERT_TRUE(strcmp(decl->var_decl.name, "x") == 0, "var name is x");
    ASSERT_TRUE(decl->var_decl.init != NULL, "has initializer");
    ASSERT_TRUE(decl->var_decl.init->kind == AST_INT_LIT, "init is int literal");
    ASSERT_TRUE(decl->var_decl.init->int_lit.value == 42, "init value is 42");

    ASSERT_TRUE(err.error_count == 0, "no parse errors");

    strtab_free(&st);
    arena_free(&arena);
}

static void test_if_statement(void) {
    Arena arena;
    Strtab st;
    ErrorCtx err;
    arena_init(&arena);
    strtab_init(&st);
    error_ctx_init(&err);

    const char *src = "int f(int x) { if (x > 0) return 1; else return 0; }";
    ASTNode *tu = parse_string(src, &arena, &st, &err);

    ASSERT_TRUE(tu != NULL, "parse returns non-null");
    ASSERT_TRUE(err.error_count == 0, "no parse errors");

    ASTNode *func = (ASTNode *)vec_get(&tu->tu.decls, 0);
    ASSERT_TRUE(func->kind == AST_FUNC_DEF, "function definition");

    strtab_free(&st);
    arena_free(&arena);
}

static void test_for_loop(void) {
    Arena arena;
    Strtab st;
    ErrorCtx err;
    arena_init(&arena);
    strtab_init(&st);
    error_ctx_init(&err);

    /* C99 for-loop with declaration */
    const char *src = "void f(void) { for (int i = 0; i < 10; i = i + 1) { } }";
    ASTNode *tu = parse_string(src, &arena, &st, &err);

    ASSERT_TRUE(tu != NULL, "parse returns non-null");
    ASSERT_TRUE(err.error_count == 0, "no parse errors for C99 for-loop");

    strtab_free(&st);
    arena_free(&arena);
}

static void test_struct(void) {
    Arena arena;
    Strtab st;
    ErrorCtx err;
    arena_init(&arena);
    strtab_init(&st);
    error_ctx_init(&err);

    const char *src = "struct Point { int x; int y; };";
    ASTNode *tu = parse_string(src, &arena, &st, &err);

    ASSERT_TRUE(tu != NULL, "parse returns non-null");
    ASSERT_TRUE(err.error_count == 0, "no parse errors for struct");

    strtab_free(&st);
    arena_free(&arena);
}

static void test_expressions(void) {
    Arena arena;
    Strtab st;
    ErrorCtx err;
    arena_init(&arena);
    strtab_init(&st);
    error_ctx_init(&err);

    const char *src = "int f(int a, int b) { return a * b + a / b - a % b; }";
    ASTNode *tu = parse_string(src, &arena, &st, &err);

    ASSERT_TRUE(tu != NULL, "parse returns non-null");
    ASSERT_TRUE(err.error_count == 0, "no parse errors for expressions");

    strtab_free(&st);
    arena_free(&arena);
}

int main(void) {
    printf("=== Parser Tests ===\n");

    test_simple_function();
    test_variable_declaration();
    test_if_statement();
    test_for_loop();
    test_struct();
    test_expressions();

    printf("\n%d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
