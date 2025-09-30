// main.c — minimal sanity tests for your custom malloc()
// NOTE: Your allocator must be linked in the same program so the symbol
// "malloc" resolves to your implementation.
//
// Build example:
//   gcc -Wall -Wextra -g -o test main.c malloc.c
// Run:
//   DEBUG_MALLOC=1 ./test   (if you want your allocator's debug prints)

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#ifndef ALIGNMENT
#define ALIGNMENT 16
#endif

static int tests_run = 0, tests_fail = 0;

static void check(int cond, const char *msg) {
    ++tests_run;
    if (!cond) {
        ++tests_fail;
        dprintf(2, "❌ FAIL: %s\n", msg);
    } else {
        dprintf(2, "✅ PASS: %s\n", msg);
    }
}

static int is_aligned(void *p, size_t alignment) {
    return ((uintptr_t)p % alignment) == 0;
}

static void test_alignment_and_rw(size_t size, unsigned char pattern) {
    void *p = malloc(size);
    char title[128];
    check(p != NULL, title);
    if (!p) return;

    check(is_aligned(p, ALIGNMENT), title);

    // write & verify within the requested size
    memset(p, pattern, size);
    unsigned char *b = (unsigned char *)p;
    int ok = 1;
    for (size_t i = 0; i < size; ++i) {
        if (b[i] != pattern) { ok = 0; break; }
    }
    check(ok, title);
}

static void test_small_sequence(void) {
    // Allocate several small chunks to exercise splitting.
    size_t sizes[] = { 1, 15, 16, 17, 31, 32, 33, 64, 128 };
    for (size_t i = 0; i < sizeof(sizes)/sizeof(sizes[0]); ++i) {
        test_alignment_and_rw(sizes[i], (unsigned char)(0x20 + i));
    }
}

static void test_large_blocks(void) {
    // Try some large sizes to force multi-page growth in your allocator.
    // Choose comfortably large values; adjust if you want even bigger.
    size_t sizes[] = {
        96 * 1024,    // ~1.5 pages if PAGE_SIZE=64K
        128 * 1024,   // ~2 pages
        512 * 1024    // ~8 pages
    };
    for (size_t i = 0; i < sizeof(sizes)/sizeof(sizes[0]); ++i) {
        test_alignment_and_rw(sizes[i], (unsigned char)(0xA0 + i));
    }
}

static void test_zero_size(void) {
    void *p = malloc(0); // your malloc() returns NULL on size==0
    check(p == NULL, "malloc(0) returns NULL");
}

int main(void)
{
    dprintf(2, "\n===== custom malloc() smoke tests =====\n");

    // Optional: turn on your allocator's debug logging for this process
    setenv("DEBUG_MALLOC", "1", 0);

    test_zero_size();
    test_small_sequence();
    test_large_blocks();

    dprintf(2, "=======================================\n");
    dprintf(2, "Tests run: %d, failures: %d\n", tests_run, tests_fail);
    if (tests_fail) {
        dprintf(2, "Result: ❌ some tests failed\n");
        return 1;
    }
    dprintf(2, "Result: ✅ all tests passed\n");
    return 0;
}
