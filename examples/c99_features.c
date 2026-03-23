/*
 * Example: C99 features showcase
 * Tests various C99 specific features
 */
#include <stdio.h>
#include <stdbool.h>

// C99: // comments (line comments)

// C99: inline functions
static inline int max(int a, int b) {
    return a > b ? a : b;
}

// C99: designated initializers
struct Point {
    int x;
    int y;
    int z;
};

// C99: variable-length arrays (VLA)
void print_matrix(int rows, int cols, int mat[rows][cols]) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            printf("%d ", mat[i][j]);
        }
        printf("\n");
    }
}

// C99: _Bool type
_Bool is_positive(int x) {
    return x > 0;
}

// C99: restrict pointer qualifier
void add_arrays(int n, int * restrict a,
                const int * restrict b, const int * restrict c) {
    for (int i = 0; i < n; i++) {
        a[i] = b[i] + c[i];
    }
}

int main(void) {
    // C99: mixed declarations and code
    printf("C99 features showcase\n");

    int x = 10;
    printf("x = %d\n", x);

    // C99: for-loop declaration
    for (int i = 0; i < 5; i++) {
        printf("i = %d\n", i);
    }

    // C99: designated initializers
    struct Point p = { .x = 1, .y = 2, .z = 3 };
    printf("Point: (%d, %d, %d)\n", p.x, p.y, p.z);

    // C99: compound literals
    struct Point *pp = &(struct Point){ .x = 10, .y = 20, .z = 30 };

    // C99: _Bool
    _Bool flag = is_positive(42);
    printf("is_positive(42) = %d\n", flag);

    // C99: long long
    long long big = 9223372036854775807LL;
    printf("big = %lld\n", big);

    // C99: hexadecimal floating-point
    double hex_float = 0x1.0p10;
    printf("hex_float = %f\n", hex_float);

    return 0;
}
