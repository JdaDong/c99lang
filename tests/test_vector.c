/*
 * c99lang - Vector (dynamic array) tests
 */
#include <stdio.h>
#include <string.h>
#include "util/vector.h"

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

static void test_vec_init_free(void) {
    Vector v;
    vec_init(&v);
    ASSERT_EQ(v.size, 0, "initial size is 0");
    ASSERT_TRUE(v.data == NULL || v.cap == 0, "initial data is empty");
    vec_free(&v);
    ASSERT_EQ(v.size, 0, "size is 0 after free");
}

static void test_vec_push_and_get(void) {
    Vector v;
    vec_init(&v);

    int a = 10, b = 20, c = 30;
    vec_push(&v, &a);
    vec_push(&v, &b);
    vec_push(&v, &c);

    ASSERT_EQ(v.size, 3, "size is 3 after 3 pushes");
    ASSERT_TRUE(vec_get(&v, 0) == &a, "get(0) returns &a");
    ASSERT_TRUE(vec_get(&v, 1) == &b, "get(1) returns &b");
    ASSERT_TRUE(vec_get(&v, 2) == &c, "get(2) returns &c");

    vec_free(&v);
}

static void test_vec_pop(void) {
    Vector v;
    vec_init(&v);

    int a = 1, b = 2, c = 3;
    vec_push(&v, &a);
    vec_push(&v, &b);
    vec_push(&v, &c);

    void *popped = vec_pop(&v);
    ASSERT_TRUE(popped == &c, "pop returns last element");
    ASSERT_EQ(v.size, 2, "size is 2 after pop");

    popped = vec_pop(&v);
    ASSERT_TRUE(popped == &b, "pop returns second element");
    ASSERT_EQ(v.size, 1, "size is 1 after second pop");

    popped = vec_pop(&v);
    ASSERT_TRUE(popped == &a, "pop returns first element");
    ASSERT_EQ(v.size, 0, "size is 0 after third pop");

    vec_free(&v);
}

static void test_vec_set(void) {
    Vector v;
    vec_init(&v);

    int a = 10, b = 20, c = 99;
    vec_push(&v, &a);
    vec_push(&v, &b);

    ASSERT_TRUE(vec_get(&v, 1) == &b, "before set: get(1) is &b");
    vec_set(&v, 1, &c);
    ASSERT_TRUE(vec_get(&v, 1) == &c, "after set: get(1) is &c");
    ASSERT_EQ(v.size, 2, "set does not change size");

    vec_free(&v);
}

static void test_vec_clear(void) {
    Vector v;
    vec_init(&v);

    int a = 1, b = 2;
    vec_push(&v, &a);
    vec_push(&v, &b);
    ASSERT_EQ(v.size, 2, "size is 2 before clear");

    vec_clear(&v);
    ASSERT_EQ(v.size, 0, "size is 0 after clear");

    /* Can push again after clear */
    int c = 3;
    vec_push(&v, &c);
    ASSERT_EQ(v.size, 1, "size is 1 after push post-clear");
    ASSERT_TRUE(vec_get(&v, 0) == &c, "get(0) after clear+push is &c");

    vec_free(&v);
}

static void test_vec_growth(void) {
    Vector v;
    vec_init(&v);

    /* Push many elements to trigger growth */
    int items[1000];
    for (int i = 0; i < 1000; i++) {
        items[i] = i;
        vec_push(&v, &items[i]);
    }
    ASSERT_EQ(v.size, 1000, "size is 1000 after 1000 pushes");

    /* Verify all elements */
    int all_correct = 1;
    for (int i = 0; i < 1000; i++) {
        int *p = (int *)vec_get(&v, i);
        if (p != &items[i]) { all_correct = 0; break; }
    }
    ASSERT_TRUE(all_correct, "all 1000 elements correct after growth");

    vec_free(&v);
}

static void test_vec_null_push(void) {
    Vector v;
    vec_init(&v);

    vec_push(&v, NULL);
    ASSERT_EQ(v.size, 1, "size is 1 after pushing NULL");
    ASSERT_TRUE(vec_get(&v, 0) == NULL, "get(0) returns NULL");

    void *p = vec_pop(&v);
    ASSERT_TRUE(p == NULL, "pop returns NULL");

    vec_free(&v);
}

static void test_vec_push_pop_sequence(void) {
    Vector v;
    vec_init(&v);

    /* Alternating push and pop */
    int a = 1, b = 2, c = 3, d = 4;
    vec_push(&v, &a);
    vec_push(&v, &b);
    ASSERT_EQ(v.size, 2, "seq: size 2");

    void *p = vec_pop(&v);
    ASSERT_TRUE(p == &b, "seq: pop b");
    ASSERT_EQ(v.size, 1, "seq: size 1");

    vec_push(&v, &c);
    vec_push(&v, &d);
    ASSERT_EQ(v.size, 3, "seq: size 3");

    ASSERT_TRUE(vec_get(&v, 0) == &a, "seq: get(0) is a");
    ASSERT_TRUE(vec_get(&v, 1) == &c, "seq: get(1) is c");
    ASSERT_TRUE(vec_get(&v, 2) == &d, "seq: get(2) is d");

    vec_free(&v);
}

static void test_vec_string_pointers(void) {
    Vector v;
    vec_init(&v);

    const char *s1 = "hello";
    const char *s2 = "world";
    const char *s3 = "test";

    vec_push(&v, (void *)s1);
    vec_push(&v, (void *)s2);
    vec_push(&v, (void *)s3);

    ASSERT_TRUE(strcmp((const char *)vec_get(&v, 0), "hello") == 0,
                "string vec: get(0) is hello");
    ASSERT_TRUE(strcmp((const char *)vec_get(&v, 1), "world") == 0,
                "string vec: get(1) is world");
    ASSERT_TRUE(strcmp((const char *)vec_get(&v, 2), "test") == 0,
                "string vec: get(2) is test");

    vec_free(&v);
}

int main(void) {
    printf("=== Vector Tests ===\n");

    test_vec_init_free();
    test_vec_push_and_get();
    test_vec_pop();
    test_vec_set();
    test_vec_clear();
    test_vec_growth();
    test_vec_null_push();
    test_vec_push_pop_sequence();
    test_vec_string_pointers();

    printf("\n%d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
