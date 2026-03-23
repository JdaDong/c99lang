/*
 * c99lang - String table (interning) tests
 */
#include <stdio.h>
#include <string.h>
#include "util/strtab.h"

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

#define ASSERT_STR_EQ(a, b, msg) do { \
    if (strcmp((a), (b)) == 0) { tests_passed++; } \
    else { tests_failed++; \
           printf("FAIL: %s (expected \"%s\", got \"%s\") at %s:%d\n", \
                  msg, (b), (a), __FILE__, __LINE__); } \
} while(0)

static void test_strtab_init_free(void) {
    Strtab st;
    strtab_init(&st);
    ASSERT_EQ(st.count, 0, "initial count is 0");
    strtab_free(&st);
}

static void test_strtab_intern_basic(void) {
    Strtab st;
    strtab_init(&st);

    const char *s1 = strtab_intern(&st, "hello", 5);
    ASSERT_TRUE(s1 != NULL, "intern returns non-null");
    ASSERT_STR_EQ(s1, "hello", "interned string equals hello");

    strtab_free(&st);
}

static void test_strtab_intern_same_string(void) {
    Strtab st;
    strtab_init(&st);

    const char *s1 = strtab_intern(&st, "hello", 5);
    const char *s2 = strtab_intern(&st, "hello", 5);

    /* Same string should return same pointer (interning) */
    ASSERT_TRUE(s1 == s2, "same string interns to same pointer");

    strtab_free(&st);
}

static void test_strtab_intern_different_strings(void) {
    Strtab st;
    strtab_init(&st);

    const char *s1 = strtab_intern(&st, "hello", 5);
    const char *s2 = strtab_intern(&st, "world", 5);

    ASSERT_TRUE(s1 != s2, "different strings have different pointers");
    ASSERT_STR_EQ(s1, "hello", "first string is hello");
    ASSERT_STR_EQ(s2, "world", "second string is world");

    strtab_free(&st);
}

static void test_strtab_intern_cstr(void) {
    Strtab st;
    strtab_init(&st);

    const char *s1 = strtab_intern_cstr(&st, "test");
    ASSERT_TRUE(s1 != NULL, "intern_cstr returns non-null");
    ASSERT_STR_EQ(s1, "test", "intern_cstr string matches");

    /* Same string via intern_cstr should return same pointer */
    const char *s2 = strtab_intern_cstr(&st, "test");
    ASSERT_TRUE(s1 == s2, "intern_cstr same string same pointer");

    /* Same string via intern with len should return same pointer */
    const char *s3 = strtab_intern(&st, "test", 4);
    ASSERT_TRUE(s1 == s3, "intern and intern_cstr same string same pointer");

    strtab_free(&st);
}

static void test_strtab_intern_substring(void) {
    Strtab st;
    strtab_init(&st);

    const char *full = "hello_world";
    const char *s1 = strtab_intern(&st, full, 5);  /* "hello" */
    ASSERT_STR_EQ(s1, "hello", "substring intern gives hello");

    /* Now intern the full string */
    const char *s2 = strtab_intern(&st, full, 11);
    ASSERT_STR_EQ(s2, "hello_world", "full string intern gives hello_world");
    ASSERT_TRUE(s1 != s2, "substring and full string are different");

    strtab_free(&st);
}

static void test_strtab_intern_empty(void) {
    Strtab st;
    strtab_init(&st);

    const char *s1 = strtab_intern(&st, "", 0);
    ASSERT_TRUE(s1 != NULL, "empty string intern returns non-null");
    ASSERT_STR_EQ(s1, "", "empty string intern is empty");

    const char *s2 = strtab_intern(&st, "", 0);
    ASSERT_TRUE(s1 == s2, "empty string interns to same pointer");

    strtab_free(&st);
}

static void test_strtab_many_strings(void) {
    Strtab st;
    strtab_init(&st);

    /* Intern many strings */
    char buf[64];
    const char *ptrs[500];
    for (int i = 0; i < 500; i++) {
        snprintf(buf, sizeof(buf), "string_%d", i);
        ptrs[i] = strtab_intern_cstr(&st, buf);
        ASSERT_TRUE(ptrs[i] != NULL, "many strings: intern non-null");
    }

    /* Verify they all intern correctly on second pass */
    int all_match = 1;
    for (int i = 0; i < 500; i++) {
        snprintf(buf, sizeof(buf), "string_%d", i);
        const char *p = strtab_intern_cstr(&st, buf);
        if (p != ptrs[i]) { all_match = 0; break; }
    }
    ASSERT_TRUE(all_match, "many strings: all re-intern to same pointers");

    strtab_free(&st);
}

static void test_strtab_similar_strings(void) {
    Strtab st;
    strtab_init(&st);

    /* Strings that differ only in the last character */
    const char *s1 = strtab_intern_cstr(&st, "testa");
    const char *s2 = strtab_intern_cstr(&st, "testb");
    const char *s3 = strtab_intern_cstr(&st, "testc");

    ASSERT_TRUE(s1 != s2, "similar strings: testa != testb");
    ASSERT_TRUE(s2 != s3, "similar strings: testb != testc");
    ASSERT_TRUE(s1 != s3, "similar strings: testa != testc");
    ASSERT_STR_EQ(s1, "testa", "similar: s1 is testa");
    ASSERT_STR_EQ(s2, "testb", "similar: s2 is testb");
    ASSERT_STR_EQ(s3, "testc", "similar: s3 is testc");

    strtab_free(&st);
}

static void test_strtab_c99_keywords(void) {
    Strtab st;
    strtab_init(&st);

    /* Intern all C99 keywords */
    const char *keywords[] = {
        "auto", "break", "case", "char", "const", "continue",
        "default", "do", "double", "else", "enum", "extern",
        "float", "for", "goto", "if", "inline", "int",
        "long", "register", "restrict", "return", "short", "signed",
        "sizeof", "static", "struct", "switch", "typedef", "union",
        "unsigned", "void", "volatile", "while",
        "_Bool", "_Complex", "_Imaginary"
    };
    int n = sizeof(keywords) / sizeof(keywords[0]);

    const char *ptrs[37];
    for (int i = 0; i < n; i++) {
        ptrs[i] = strtab_intern_cstr(&st, keywords[i]);
        ASSERT_TRUE(ptrs[i] != NULL, "keyword intern non-null");
    }

    /* Re-intern and check */
    int all_match = 1;
    for (int i = 0; i < n; i++) {
        const char *p = strtab_intern_cstr(&st, keywords[i]);
        if (p != ptrs[i]) { all_match = 0; break; }
    }
    ASSERT_TRUE(all_match, "all keywords re-intern to same pointers");

    strtab_free(&st);
}

int main(void) {
    printf("=== Strtab Tests ===\n");

    test_strtab_init_free();
    test_strtab_intern_basic();
    test_strtab_intern_same_string();
    test_strtab_intern_different_strings();
    test_strtab_intern_cstr();
    test_strtab_intern_substring();
    test_strtab_intern_empty();
    test_strtab_many_strings();
    test_strtab_similar_strings();
    test_strtab_c99_keywords();

    printf("\n%d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
