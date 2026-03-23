/*
 * c99lang - Extended Parser tests
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

static ASTNode *parse_src(const char *src, Arena *a, Strtab *st, ErrorCtx *err) {
    Lexer lex;
    lexer_init(&lex, src, strlen(src), "test.c", st, err);
    Parser parser;
    parser_init(&parser, &lex, a, st, err);
    return parser_parse(&parser);
}

#define BEGIN Arena a; Strtab st; ErrorCtx err; \
    arena_init(&a); strtab_init(&st); error_ctx_init(&err);
#define END strtab_free(&st); arena_free(&a);

static void test_while_loop(void) {
    BEGIN
    ASTNode *tu = parse_src("void f(void) { while (1) { break; continue; } }", &a, &st, &err);
    ASSERT_TRUE(tu != NULL, "while: non-null");
    ASSERT_TRUE(err.error_count == 0, "while: no errors");
    END
}

static void test_do_while(void) {
    BEGIN
    ASTNode *tu = parse_src("void f(void) { do { int x = 1; } while (x > 0); }", &a, &st, &err);
    ASSERT_TRUE(tu != NULL, "do-while: non-null");
    ASSERT_TRUE(err.error_count == 0, "do-while: no errors");
    END
}

static void test_switch_case(void) {
    BEGIN
    ASTNode *tu = parse_src(
        "int f(int x) { switch(x) { case 0: return 0; case 1: return 1; default: return -1; } }",
        &a, &st, &err);
    ASSERT_TRUE(tu != NULL, "switch: non-null");
    ASSERT_TRUE(err.error_count == 0, "switch: no errors");
    END
}

static void test_goto_label(void) {
    BEGIN
    ASTNode *tu = parse_src("void f(void) { goto end; int x = 1; end: return; }", &a, &st, &err);
    ASSERT_TRUE(tu != NULL, "goto: non-null");
    ASSERT_TRUE(err.error_count == 0, "goto: no errors");
    END
}

static void test_multi_params(void) {
    BEGIN
    ASTNode *tu = parse_src("int add(int a, int b, int c) { return a + b + c; }", &a, &st, &err);
    ASSERT_TRUE(tu != NULL, "multi params: non-null");
    ASSERT_TRUE(err.error_count == 0, "multi params: no errors");
    ASTNode *func = (ASTNode *)vec_get(&tu->tu.decls, 0);
    ASSERT_TRUE(func->func_def.params.size == 3, "multi params: 3 params");
    END
}

static void test_pointer_decl(void) {
    BEGIN
    ASTNode *tu = parse_src("int *p; int **pp; const int *cp;", &a, &st, &err);
    ASSERT_TRUE(tu != NULL, "ptr decl: non-null");
    ASSERT_TRUE(err.error_count == 0, "ptr decl: no errors");
    ASSERT_TRUE(tu->tu.decls.size == 3, "ptr decl: 3 decls");
    END
}

static void test_array_decl(void) {
    BEGIN
    ASTNode *tu = parse_src("int arr[10]; int matrix[3][4];", &a, &st, &err);
    ASSERT_TRUE(tu != NULL, "array decl: non-null");
    ASSERT_TRUE(err.error_count == 0, "array decl: no errors");
    END
}

static void test_enum_decl(void) {
    BEGIN
    ASTNode *tu = parse_src("enum Color { RED, GREEN, BLUE = 5, ALPHA };", &a, &st, &err);
    ASSERT_TRUE(tu != NULL, "enum: non-null");
    ASSERT_TRUE(err.error_count == 0, "enum: no errors");
    END
}

static void test_union_decl(void) {
    BEGIN
    ASTNode *tu = parse_src("union Data { int i; float f; char c; };", &a, &st, &err);
    ASSERT_TRUE(tu != NULL, "union: non-null");
    ASSERT_TRUE(err.error_count == 0, "union: no errors");
    END
}

static void test_ternary_expr(void) {
    BEGIN
    ASTNode *tu = parse_src("int f(int x) { return x > 0 ? x : -x; }", &a, &st, &err);
    ASSERT_TRUE(tu != NULL, "ternary: non-null");
    ASSERT_TRUE(err.error_count == 0, "ternary: no errors");
    END
}

static void test_cast_expr(void) {
    BEGIN
    ASTNode *tu = parse_src("void f(void) { int x = (int)3.14; float y = (float)x; }", &a, &st, &err);
    ASSERT_TRUE(tu != NULL, "cast: non-null");
    ASSERT_TRUE(err.error_count == 0, "cast: no errors");
    END
}

static void test_sizeof_expr(void) {
    BEGIN
    ASTNode *tu = parse_src("void f(void) { int s1 = sizeof(int); int x; int s2 = sizeof x; }", &a, &st, &err);
    ASSERT_TRUE(tu != NULL, "sizeof: non-null");
    ASSERT_TRUE(err.error_count == 0, "sizeof: no errors");
    END
}

static void test_compound_assign(void) {
    BEGIN
    ASTNode *tu = parse_src(
        "void f(int x) { x += 1; x -= 2; x *= 3; x /= 4; x %= 5; "
        "x <<= 1; x >>= 2; x &= 0xFF; x ^= 0x0F; x |= 0x80; }", &a, &st, &err);
    ASSERT_TRUE(tu != NULL, "compound assign: non-null");
    ASSERT_TRUE(err.error_count == 0, "compound assign: no errors");
    END
}

static void test_unary_ops(void) {
    BEGIN
    ASTNode *tu = parse_src(
        "void f(void) { int x = 5; int a = -x; int b = +x; "
        "int c = !x; int d = ~x; int *p = &x; int e = *p; }", &a, &st, &err);
    ASSERT_TRUE(tu != NULL, "unary: non-null");
    ASSERT_TRUE(err.error_count == 0, "unary: no errors");
    END
}

static void test_pre_post_increment(void) {
    BEGIN
    ASTNode *tu = parse_src("void f(void) { int x = 0; ++x; --x; x++; x--; }", &a, &st, &err);
    ASSERT_TRUE(tu != NULL, "inc/dec: non-null");
    ASSERT_TRUE(err.error_count == 0, "inc/dec: no errors");
    END
}

static void test_function_call(void) {
    BEGIN
    ASTNode *tu = parse_src("int foo(int x, int y);\nvoid bar(void) { int r = foo(1, 2); }", &a, &st, &err);
    ASSERT_TRUE(tu != NULL, "func call: non-null");
    ASSERT_TRUE(err.error_count == 0, "func call: no errors");
    ASSERT_TRUE(tu->tu.decls.size == 2, "func call: 2 decls");
    END
}

static void test_array_index(void) {
    BEGIN
    ASTNode *tu = parse_src("void f(void) { int arr[5]; int x = arr[0]; arr[2] = 42; }", &a, &st, &err);
    ASSERT_TRUE(tu != NULL, "array index: non-null");
    ASSERT_TRUE(err.error_count == 0, "array index: no errors");
    END
}

static void test_member_access(void) {
    BEGIN
    ASTNode *tu = parse_src(
        "struct S { int x; int y; };\n"
        "void f(void) { struct S s; s.x = 1; s.y = 2; }", &a, &st, &err);
    ASSERT_TRUE(tu != NULL, "member access: non-null");
    ASSERT_TRUE(err.error_count == 0, "member access: no errors");
    END
}

static void test_comma_expr(void) {
    BEGIN
    ASTNode *tu = parse_src("void f(void) { int x; x = (1, 2, 3); }", &a, &st, &err);
    ASSERT_TRUE(tu != NULL, "comma expr: non-null");
    ASSERT_TRUE(err.error_count == 0, "comma expr: no errors");
    END
}

static void test_nested_for(void) {
    BEGIN
    ASTNode *tu = parse_src(
        "void f(void) { for(int i=0;i<10;i=i+1) { for(int j=0;j<10;j=j+1) { } } }",
        &a, &st, &err);
    ASSERT_TRUE(tu != NULL, "nested for: non-null");
    ASSERT_TRUE(err.error_count == 0, "nested for: no errors");
    END
}

static void test_empty_function(void) {
    BEGIN
    ASTNode *tu = parse_src("void f(void) { }", &a, &st, &err);
    ASSERT_TRUE(tu != NULL, "empty func: non-null");
    ASSERT_TRUE(err.error_count == 0, "empty func: no errors");
    END
}

static void test_multiple_functions(void) {
    BEGIN
    ASTNode *tu = parse_src(
        "int foo(void) { return 1; }\n"
        "int bar(void) { return 2; }\n"
        "int baz(void) { return 3; }", &a, &st, &err);
    ASSERT_TRUE(tu != NULL, "multi func: non-null");
    ASSERT_TRUE(err.error_count == 0, "multi func: no errors");
    ASSERT_TRUE(tu->tu.decls.size == 3, "multi func: 3 functions");
    END
}

static void test_extern_decl(void) {
    BEGIN
    ASTNode *tu = parse_src("extern int global_var; extern void ext_func(int x);", &a, &st, &err);
    ASSERT_TRUE(tu != NULL, "extern: non-null");
    ASSERT_TRUE(err.error_count == 0, "extern: no errors");
    END
}

static void test_static_function(void) {
    BEGIN
    ASTNode *tu = parse_src("static int helper(int x) { return x * 2; }", &a, &st, &err);
    ASSERT_TRUE(tu != NULL, "static func: non-null");
    ASSERT_TRUE(err.error_count == 0, "static func: no errors");
    END
}

static void test_complex_expression_parsing(void) {
    BEGIN
    ASTNode *tu = parse_src(
        "int f(int a, int b, int c) { return (a + b) * c - a / (b + 1) + c % 2; }",
        &a, &st, &err);
    ASSERT_TRUE(tu != NULL, "complex expr: non-null");
    ASSERT_TRUE(err.error_count == 0, "complex expr: no errors");
    END
}

static void test_string_variable(void) {
    BEGIN
    ASTNode *tu = parse_src("char *msg = \"hello world\";", &a, &st, &err);
    ASSERT_TRUE(tu != NULL, "string var: non-null");
    ASSERT_TRUE(err.error_count == 0, "string var: no errors");
    ASTNode *decl = (ASTNode *)vec_get(&tu->tu.decls, 0);
    ASSERT_TRUE(decl->kind == AST_VAR_DECL, "string var: var decl");
    ASSERT_TRUE(decl->var_decl.init != NULL, "string var: has init");
    ASSERT_TRUE(decl->var_decl.init->kind == AST_STRING_LIT, "string var: init is string");
    END
}

static void test_null_statement(void) {
    BEGIN
    ASTNode *tu = parse_src("void f(void) { ; ; ; }", &a, &st, &err);
    ASSERT_TRUE(tu != NULL, "null stmt: non-null");
    ASSERT_TRUE(err.error_count == 0, "null stmt: no errors");
    END
}

static void test_if_else_chain(void) {
    BEGIN
    ASTNode *tu = parse_src(
        "int f(int x) { if(x==1) return 1; else if(x==2) return 2; else if(x==3) return 3; else return 0; }",
        &a, &st, &err);
    ASSERT_TRUE(tu != NULL, "if-else chain: non-null");
    ASSERT_TRUE(err.error_count == 0, "if-else chain: no errors");
    END
}

static void test_mixed_declarations_and_code(void) {
    BEGIN
    /* C99 allows mixing declarations and statements */
    ASTNode *tu = parse_src(
        "void f(void) { int a = 1; a = a + 1; int b = 2; b = b + a; int c = a + b; }",
        &a, &st, &err);
    ASSERT_TRUE(tu != NULL, "mixed decl/code: non-null");
    ASSERT_TRUE(err.error_count == 0, "mixed decl/code: no errors");
    END
}

static void test_variadic_function(void) {
    BEGIN
    ASTNode *tu = parse_src("int printf(const char *fmt, ...);", &a, &st, &err);
    ASSERT_TRUE(tu != NULL, "variadic: non-null");
    ASSERT_TRUE(err.error_count == 0, "variadic: no errors");
    END
}

int main(void) {
    printf("=== Extended Parser Tests ===\n");

    test_while_loop();
    test_do_while();
    test_switch_case();
    test_goto_label();
    test_multi_params();
    test_pointer_decl();
    test_array_decl();
    test_enum_decl();
    test_union_decl();
    test_ternary_expr();
    test_cast_expr();
    test_sizeof_expr();
    test_compound_assign();
    test_unary_ops();
    test_pre_post_increment();
    test_function_call();
    test_array_index();
    test_member_access();
    test_comma_expr();
    test_nested_for();
    test_empty_function();
    test_multiple_functions();
    test_extern_decl();
    test_static_function();
    test_complex_expression_parsing();
    test_string_variable();
    test_null_statement();
    test_if_else_chain();
    test_mixed_declarations_and_code();
    test_variadic_function();

    printf("\n%d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
