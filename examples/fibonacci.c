/*
 * Example: Fibonacci with recursion and iteration
 */
int printf(const char *fmt, ...);

int fib_recursive(int n) {
    if (n <= 1) return n;
    return fib_recursive(n - 1) + fib_recursive(n - 2);
}

int fib_iterative(int n) {
    if (n <= 1) return n;

    int prev = 0;
    int curr = 1;

    for (int i = 2; i <= n; i++) {
        int next = prev + curr;
        prev = curr;
        curr = next;
    }

    return curr;
}

int main(void) {
    printf("Fibonacci numbers:\n");

    for (int i = 0; i < 20; i++) {
        printf("fib(%d) = %d (recursive), %d (iterative)\n",
               i, fib_recursive(i), fib_iterative(i));
    }

    return 0;
}
