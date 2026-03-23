/*
 * c99lang - Type system and Symbol table tests
 */
#include <stdio.h>
#include <string.h>
#include "sema/type.h"
#include "sema/symtab.h"
#include "util/arena.h"
#include "util/error.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_TRUE(cond, msg) do { \
    if (cond) { tests_passed++; } \
    else { tests_failed++; \
           printf("FAIL: %s at %s:%d\n", msg, __FILE__, __LINE__); } \
} while(0)

#define ASSERT_EQ(a, b, msg) do { \
    if ((long)(a) == (long)(b)) { tests_passed++; } \
    else { tests_failed++; \
           printf("FAIL: %s (expected %ld, got %ld) at %s:%d\n", \
                  msg, (long)(b), (long)(a), __FILE__, __LINE__); } \
} while(0)

/* ---- Type system tests ---- */

static void test_type_singletons(void) {
    CType *v1 = type_void_get();
    CType *v2 = type_void_get();
    ASSERT_TRUE(v1 == v2, "void singleton same pointer");
    ASSERT_EQ(v1->kind, TYPE_VOID, "void kind");

    CType *i1 = type_int_get();
    CType *i2 = type_int_get();
    ASSERT_TRUE(i1 == i2, "int singleton same pointer");
    ASSERT_EQ(i1->kind, TYPE_INT, "int kind");
    ASSERT_EQ(i1->size, 4, "int size is 4");

    ASSERT_EQ(type_char_get()->size, 1, "char size is 1");
    ASSERT_EQ(type_long_get()->size, 8, "long size is 8");
    ASSERT_EQ(type_float_get()->size, 4, "float size is 4");
    ASSERT_EQ(type_double_get()->size, 8, "double size is 8");
}

static void test_type_new_returns_singleton(void) {
    Arena arena;
    arena_init(&arena);

    CType *t = type_new(&arena, TYPE_INT);
    ASSERT_TRUE(t == type_int_get(), "type_new(INT) returns singleton");

    CType *t2 = type_new(&arena, TYPE_VOID);
    ASSERT_TRUE(t2 == type_void_get(), "type_new(VOID) returns singleton");

    arena_free(&arena);
}

static void test_type_ptr(void) {
    Arena arena;
    arena_init(&arena);

    CType *int_ptr = type_ptr(&arena, type_int_get(), 0);
    ASSERT_EQ(int_ptr->kind, TYPE_POINTER, "ptr kind");
    ASSERT_TRUE(int_ptr->ptr.pointee == type_int_get(), "ptr pointee is int");
    ASSERT_EQ(int_ptr->size, 8, "ptr size is 8");
    ASSERT_EQ(int_ptr->align, 8, "ptr align is 8");

    /* Pointer to pointer */
    CType *pp = type_ptr(&arena, int_ptr, 0);
    ASSERT_EQ(pp->kind, TYPE_POINTER, "ptr-to-ptr kind");
    ASSERT_TRUE(pp->ptr.pointee == int_ptr, "ptr-to-ptr pointee");

    /* Const pointer */
    CType *cp = type_ptr(&arena, type_int_get(), QUAL_CONST);
    ASSERT_EQ(cp->quals & QUAL_CONST, QUAL_CONST, "const qualifier");

    arena_free(&arena);
}

static void test_type_array(void) {
    Arena arena;
    arena_init(&arena);

    CType *arr = type_array(&arena, type_int_get(), 10);
    ASSERT_EQ(arr->kind, TYPE_ARRAY, "array kind");
    ASSERT_TRUE(arr->array.elem == type_int_get(), "array elem is int");
    ASSERT_EQ(arr->array.count, 10, "array count is 10");
    ASSERT_EQ(arr->size, 40, "array size is 40 (10*4)");
    ASSERT_EQ(arr->array.is_vla, 0, "array is not VLA");

    /* Array of arrays */
    CType *matrix = type_array(&arena, arr, 5);
    ASSERT_EQ(matrix->kind, TYPE_ARRAY, "matrix kind");
    ASSERT_EQ(matrix->array.count, 5, "matrix count is 5");
    ASSERT_EQ(matrix->size, 200, "matrix size is 200 (5*40)");

    arena_free(&arena);
}

static void test_type_func(void) {
    Arena arena;
    arena_init(&arena);

    CTypeParam params[2];
    params[0].name = "a";
    params[0].type = type_int_get();
    params[1].name = "b";
    params[1].type = type_int_get();

    CType *func = type_func(&arena, type_int_get(), params, 2, false);
    ASSERT_EQ(func->kind, TYPE_FUNC, "func kind");
    ASSERT_TRUE(func->func.ret == type_int_get(), "func ret is int");
    ASSERT_EQ(func->func.param_count, 2, "func has 2 params");
    ASSERT_EQ(func->func.is_variadic, 0, "func not variadic");

    /* Variadic function */
    CType *vfunc = type_func(&arena, type_int_get(), params, 1, true);
    ASSERT_EQ(vfunc->func.is_variadic, 1, "variadic func is variadic");

    arena_free(&arena);
}

static void test_type_is_integer(void) {
    ASSERT_TRUE(type_is_integer(type_int_get()), "int is integer");
    ASSERT_TRUE(type_is_integer(type_char_get()), "char is integer");
    ASSERT_TRUE(type_is_integer(type_long_get()), "long is integer");
    ASSERT_TRUE(type_is_integer(type_uint_get()), "uint is integer");
    ASSERT_TRUE(type_is_integer(type_bool_get()), "bool is integer");
    ASSERT_TRUE(type_is_integer(type_llong_get()), "llong is integer");
    ASSERT_TRUE(type_is_integer(type_ullong_get()), "ullong is integer");

    ASSERT_TRUE(!type_is_integer(type_float_get()), "float is NOT integer");
    ASSERT_TRUE(!type_is_integer(type_double_get()), "double is NOT integer");
    ASSERT_TRUE(!type_is_integer(type_void_get()), "void is NOT integer");
    ASSERT_TRUE(!type_is_integer(NULL), "NULL is NOT integer");
}

static void test_type_is_floating(void) {
    ASSERT_TRUE(type_is_floating(type_float_get()), "float is floating");
    ASSERT_TRUE(type_is_floating(type_double_get()), "double is floating");

    ASSERT_TRUE(!type_is_floating(type_int_get()), "int is NOT floating");
    ASSERT_TRUE(!type_is_floating(type_void_get()), "void is NOT floating");
    ASSERT_TRUE(!type_is_floating(NULL), "NULL is NOT floating");
}

static void test_type_is_arithmetic(void) {
    ASSERT_TRUE(type_is_arithmetic(type_int_get()), "int is arithmetic");
    ASSERT_TRUE(type_is_arithmetic(type_float_get()), "float is arithmetic");
    ASSERT_TRUE(type_is_arithmetic(type_double_get()), "double is arithmetic");
    ASSERT_TRUE(type_is_arithmetic(type_char_get()), "char is arithmetic");

    ASSERT_TRUE(!type_is_arithmetic(type_void_get()), "void not arithmetic");
}

static void test_type_is_scalar(void) {
    Arena arena;
    arena_init(&arena);

    ASSERT_TRUE(type_is_scalar(type_int_get()), "int is scalar");
    ASSERT_TRUE(type_is_scalar(type_float_get()), "float is scalar");

    CType *ptr = type_ptr(&arena, type_int_get(), 0);
    ASSERT_TRUE(type_is_scalar(ptr), "pointer is scalar");

    ASSERT_TRUE(!type_is_scalar(type_void_get()), "void not scalar");

    arena_free(&arena);
}

static void test_type_is_complete(void) {
    Arena arena;
    arena_init(&arena);

    ASSERT_TRUE(type_is_complete(type_int_get()), "int is complete");
    ASSERT_TRUE(!type_is_complete(type_void_get()), "void is NOT complete");

    CType *arr = type_array(&arena, type_int_get(), 5);
    ASSERT_TRUE(type_is_complete(arr), "int[5] is complete");

    CType *empty_arr = type_array(&arena, type_int_get(), 0);
    ASSERT_TRUE(!type_is_complete(empty_arr), "int[0] is NOT complete");

    arena_free(&arena);
}

static void test_type_compatible(void) {
    Arena arena;
    arena_init(&arena);

    ASSERT_TRUE(type_is_compatible(type_int_get(), type_int_get()), "int compat int");
    ASSERT_TRUE(!type_is_compatible(type_int_get(), type_float_get()), "int !compat float");
    ASSERT_TRUE(!type_is_compatible(NULL, type_int_get()), "NULL !compat int");
    ASSERT_TRUE(!type_is_compatible(type_int_get(), NULL), "int !compat NULL");

    CType *p1 = type_ptr(&arena, type_int_get(), 0);
    CType *p2 = type_ptr(&arena, type_int_get(), 0);
    CType *p3 = type_ptr(&arena, type_char_get(), 0);
    ASSERT_TRUE(type_is_compatible(p1, p2), "int* compat int*");
    ASSERT_TRUE(!type_is_compatible(p1, p3), "int* !compat char*");

    arena_free(&arena);
}

/* ---- Symbol table tests ---- */

static void test_symtab_init(void) {
    Arena arena;
    arena_init(&arena);

    SymTab st;
    symtab_init(&st, &arena);
    ASSERT_EQ(st.scope_depth, 0, "initial scope depth is 0");

    arena_free(&arena);
}

static void test_symtab_insert_lookup(void) {
    Arena arena;
    arena_init(&arena);

    SymTab st;
    symtab_init(&st, &arena);
    SrcLoc loc = {"test.c", 1, 1};

    Symbol *s = symtab_insert(&st, "x", SYM_VAR, type_int_get(), loc);
    ASSERT_TRUE(s != NULL, "insert returns non-null");
    ASSERT_TRUE(strcmp(s->name, "x") == 0, "inserted symbol name is x");
    ASSERT_EQ(s->kind, SYM_VAR, "symbol kind is VAR");

    Symbol *found = symtab_lookup(&st, "x");
    ASSERT_TRUE(found != NULL, "lookup finds x");
    ASSERT_TRUE(found == s, "lookup returns same symbol");

    Symbol *not_found = symtab_lookup(&st, "y");
    ASSERT_TRUE(not_found == NULL, "lookup y returns NULL");

    arena_free(&arena);
}

static void test_symtab_multiple_symbols(void) {
    Arena arena;
    arena_init(&arena);

    SymTab st;
    symtab_init(&st, &arena);
    SrcLoc loc = {"test.c", 1, 1};

    symtab_insert(&st, "a", SYM_VAR, type_int_get(), loc);
    symtab_insert(&st, "b", SYM_VAR, type_float_get(), loc);
    symtab_insert(&st, "c", SYM_FUNC, type_int_get(), loc);

    Symbol *a = symtab_lookup(&st, "a");
    Symbol *b = symtab_lookup(&st, "b");
    Symbol *c = symtab_lookup(&st, "c");

    ASSERT_TRUE(a != NULL && strcmp(a->name, "a") == 0, "found a");
    ASSERT_TRUE(b != NULL && strcmp(b->name, "b") == 0, "found b");
    ASSERT_TRUE(c != NULL && strcmp(c->name, "c") == 0, "found c");
    ASSERT_EQ(c->kind, SYM_FUNC, "c is function");

    arena_free(&arena);
}

static void test_symtab_scopes(void) {
    Arena arena;
    arena_init(&arena);

    SymTab st;
    symtab_init(&st, &arena);
    SrcLoc loc = {"test.c", 1, 1};

    /* Global scope: insert x */
    symtab_insert(&st, "x", SYM_VAR, type_int_get(), loc);

    /* Push inner scope: insert y, shadow x */
    symtab_push_scope(&st);
    symtab_insert(&st, "y", SYM_VAR, type_float_get(), loc);
    symtab_insert(&st, "x", SYM_VAR, type_double_get(), loc);

    /* Inner x should be found (shadows outer) */
    Symbol *inner_x = symtab_lookup(&st, "x");
    ASSERT_TRUE(inner_x != NULL, "inner x found");
    ASSERT_TRUE(inner_x->type == type_double_get(), "inner x is double (shadowed)");

    Symbol *y = symtab_lookup(&st, "y");
    ASSERT_TRUE(y != NULL, "y found in inner scope");

    /* Pop inner scope */
    symtab_pop_scope(&st);

    /* Outer x should now be found */
    Symbol *outer_x = symtab_lookup(&st, "x");
    ASSERT_TRUE(outer_x != NULL, "outer x found after pop");
    ASSERT_TRUE(outer_x->type == type_int_get(), "outer x is int");

    /* y should no longer be visible */
    Symbol *y_after = symtab_lookup(&st, "y");
    ASSERT_TRUE(y_after == NULL, "y not found after pop");

    arena_free(&arena);
}

static void test_symtab_nested_scopes(void) {
    Arena arena;
    arena_init(&arena);

    SymTab st;
    symtab_init(&st, &arena);
    SrcLoc loc = {"test.c", 1, 1};

    /* scope 0 */
    symtab_insert(&st, "a", SYM_VAR, type_int_get(), loc);

    symtab_push_scope(&st); /* scope 1 */
    symtab_insert(&st, "b", SYM_VAR, type_int_get(), loc);

    symtab_push_scope(&st); /* scope 2 */
    symtab_insert(&st, "c", SYM_VAR, type_int_get(), loc);

    ASSERT_TRUE(symtab_lookup(&st, "a") != NULL, "a visible at depth 2");
    ASSERT_TRUE(symtab_lookup(&st, "b") != NULL, "b visible at depth 2");
    ASSERT_TRUE(symtab_lookup(&st, "c") != NULL, "c visible at depth 2");

    symtab_pop_scope(&st); /* back to scope 1 */
    ASSERT_TRUE(symtab_lookup(&st, "a") != NULL, "a visible at depth 1");
    ASSERT_TRUE(symtab_lookup(&st, "b") != NULL, "b visible at depth 1");
    ASSERT_TRUE(symtab_lookup(&st, "c") == NULL, "c not visible at depth 1");

    symtab_pop_scope(&st); /* back to scope 0 */
    ASSERT_TRUE(symtab_lookup(&st, "a") != NULL, "a visible at depth 0");
    ASSERT_TRUE(symtab_lookup(&st, "b") == NULL, "b not visible at depth 0");
    ASSERT_TRUE(symtab_lookup(&st, "c") == NULL, "c not visible at depth 0");

    arena_free(&arena);
}

static void test_symtab_lookup_current_scope(void) {
    Arena arena;
    arena_init(&arena);

    SymTab st;
    symtab_init(&st, &arena);
    SrcLoc loc = {"test.c", 1, 1};

    symtab_insert(&st, "x", SYM_VAR, type_int_get(), loc);
    symtab_push_scope(&st);

    /* x exists in outer scope but not current */
    ASSERT_TRUE(symtab_lookup(&st, "x") != NULL, "x found via lookup");
    ASSERT_TRUE(symtab_lookup_current_scope(&st, "x") == NULL,
                "x NOT found in current scope");

    /* Insert x in current scope */
    symtab_insert(&st, "x", SYM_VAR, type_float_get(), loc);
    ASSERT_TRUE(symtab_lookup_current_scope(&st, "x") != NULL,
                "x found in current scope after insert");

    symtab_pop_scope(&st);
    arena_free(&arena);
}

static void test_symtab_enum_const(void) {
    Arena arena;
    arena_init(&arena);

    SymTab st;
    symtab_init(&st, &arena);
    SrcLoc loc = {"test.c", 1, 1};

    Symbol *s = symtab_insert(&st, "RED", SYM_ENUM_CONST, type_int_get(), loc);
    s->enum_value = 0;

    Symbol *found = symtab_lookup(&st, "RED");
    ASSERT_TRUE(found != NULL, "enum const found");
    ASSERT_EQ(found->kind, SYM_ENUM_CONST, "kind is ENUM_CONST");
    ASSERT_EQ(found->enum_value, 0, "enum value is 0");

    arena_free(&arena);
}

int main(void) {
    printf("=== Type System & Symbol Table Tests ===\n");

    /* Type system */
    test_type_singletons();
    test_type_new_returns_singleton();
    test_type_ptr();
    test_type_array();
    test_type_func();
    test_type_is_integer();
    test_type_is_floating();
    test_type_is_arithmetic();
    test_type_is_scalar();
    test_type_is_complete();
    test_type_compatible();

    /* Symbol table */
    test_symtab_init();
    test_symtab_insert_lookup();
    test_symtab_multiple_symbols();
    test_symtab_scopes();
    test_symtab_nested_scopes();
    test_symtab_lookup_current_scope();
    test_symtab_enum_const();

    printf("\n%d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
