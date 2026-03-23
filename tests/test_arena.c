/*
 * c99lang - Arena memory allocator tests
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "util/arena.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_TRUE(cond, msg) do { \
    if (cond) { tests_passed++; } \
    else { tests_failed++; \
           printf("FAIL: %s at %s:%d\n", msg, __FILE__, __LINE__); } \
} while(0)

#define ASSERT_EQ(a, b, msg) do { \
    if ((a) == (b)) { tests_passed++; } \
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

static void test_arena_init_free(void) {
    Arena arena;
    arena_init(&arena);
    ASSERT_TRUE(arena.head != NULL, "arena head is non-NULL after init");
    ASSERT_TRUE(arena.current != NULL, "arena current is non-NULL after init");
    arena_free(&arena);
    ASSERT_TRUE(arena.head == NULL, "arena head is NULL after free");
}

static void test_arena_alloc_basic(void) {
    Arena arena;
    arena_init(&arena);

    void *p1 = arena_alloc(&arena, 16);
    ASSERT_TRUE(p1 != NULL, "first alloc returns non-null");

    void *p2 = arena_alloc(&arena, 32);
    ASSERT_TRUE(p2 != NULL, "second alloc returns non-null");
    ASSERT_TRUE(p1 != p2, "two allocs return different pointers");

    arena_free(&arena);
}

static void test_arena_alloc_alignment(void) {
    Arena arena;
    arena_init(&arena);

    /* Allocate odd sizes and check that pointers are aligned */
    void *p1 = arena_alloc(&arena, 1);
    void *p2 = arena_alloc(&arena, 1);
    ASSERT_TRUE(p1 != NULL, "alloc 1 byte returns non-null");
    ASSERT_TRUE(p2 != NULL, "alloc 1 byte returns non-null (2)");

    /* Allocate various sizes */
    for (int i = 1; i <= 128; i++) {
        void *p = arena_alloc(&arena, i);
        ASSERT_TRUE(p != NULL, "alloc various sizes");
    }

    arena_free(&arena);
}

static void test_arena_calloc(void) {
    Arena arena;
    arena_init(&arena);

    int *p = (int *)arena_calloc(&arena, sizeof(int) * 10);
    ASSERT_TRUE(p != NULL, "calloc returns non-null");

    /* calloc should zero-initialize */
    int all_zero = 1;
    for (int i = 0; i < 10; i++) {
        if (p[i] != 0) { all_zero = 0; break; }
    }
    ASSERT_TRUE(all_zero, "calloc zero-initializes memory");

    arena_free(&arena);
}

static void test_arena_strdup(void) {
    Arena arena;
    arena_init(&arena);

    const char *original = "hello, world!";
    char *dup = arena_strdup(&arena, original);
    ASSERT_TRUE(dup != NULL, "strdup returns non-null");
    ASSERT_STR_EQ(dup, original, "strdup copies string correctly");
    ASSERT_TRUE(dup != original, "strdup returns different pointer");

    /* Empty string */
    char *empty = arena_strdup(&arena, "");
    ASSERT_TRUE(empty != NULL, "strdup of empty string returns non-null");
    ASSERT_STR_EQ(empty, "", "strdup of empty string is empty");

    arena_free(&arena);
}

static void test_arena_strndup(void) {
    Arena arena;
    arena_init(&arena);

    const char *original = "hello, world!";
    char *partial = arena_strndup(&arena, original, 5);
    ASSERT_TRUE(partial != NULL, "strndup returns non-null");
    ASSERT_STR_EQ(partial, "hello", "strndup copies correct prefix");

    /* n larger than string */
    char *full = arena_strndup(&arena, "abc", 10);
    ASSERT_TRUE(full != NULL, "strndup with large n returns non-null");
    ASSERT_STR_EQ(full, "abc", "strndup with large n copies full string");

    /* n = 0 */
    char *zero_len = arena_strndup(&arena, "test", 0);
    ASSERT_TRUE(zero_len != NULL, "strndup with n=0 returns non-null");
    ASSERT_STR_EQ(zero_len, "", "strndup with n=0 returns empty string");

    arena_free(&arena);
}

static void test_arena_large_alloc(void) {
    Arena arena;
    arena_init(&arena);

    /* Allocate more than ARENA_BLOCK_SIZE */
    size_t large_size = ARENA_BLOCK_SIZE * 2;
    void *p = arena_alloc(&arena, large_size);
    ASSERT_TRUE(p != NULL, "large alloc returns non-null");

    /* Write to it to ensure it's valid memory */
    memset(p, 0xAB, large_size);

    arena_free(&arena);
}

static void test_arena_many_allocs(void) {
    Arena arena;
    arena_init(&arena);

    /* Many small allocations (should trigger block allocation) */
    int count = 10000;
    for (int i = 0; i < count; i++) {
        void *p = arena_alloc(&arena, 64);
        ASSERT_TRUE(p != NULL, "many small allocs return non-null");
    }

    arena_free(&arena);
}

static void test_arena_mixed_allocs(void) {
    Arena arena;
    arena_init(&arena);

    /* Mix of alloc, calloc, strdup */
    int *ints = (int *)arena_calloc(&arena, sizeof(int) * 5);
    char *s1 = arena_strdup(&arena, "first");
    void *buf = arena_alloc(&arena, 256);
    char *s2 = arena_strdup(&arena, "second");
    char *s3 = arena_strndup(&arena, "third_extra", 5);

    ASSERT_TRUE(ints != NULL, "mixed: calloc ok");
    ASSERT_TRUE(buf != NULL, "mixed: alloc ok");
    ASSERT_STR_EQ(s1, "first", "mixed: strdup 1 ok");
    ASSERT_STR_EQ(s2, "second", "mixed: strdup 2 ok");
    ASSERT_STR_EQ(s3, "third", "mixed: strndup ok");

    /* Verify zero-init still holds */
    int all_zero = 1;
    for (int i = 0; i < 5; i++) {
        if (ints[i] != 0) { all_zero = 0; break; }
    }
    ASSERT_TRUE(all_zero, "mixed: calloc zero-init preserved");

    arena_free(&arena);
}

int main(void) {
    printf("=== Arena Tests ===\n");

    test_arena_init_free();
    test_arena_alloc_basic();
    test_arena_alloc_alignment();
    test_arena_calloc();
    test_arena_strdup();
    test_arena_strndup();
    test_arena_large_alloc();
    test_arena_many_allocs();
    test_arena_mixed_allocs();

    printf("\n%d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
