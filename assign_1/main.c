// main.c — allocator + libpp sanity harness
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pp.h>          // from /home/jgonz555/OS/assign_1/given/Asgn1/include

static void banner(const char *msg) {
    // to stderr so it won't mix with pp()
    dprintf(2, "\n===== %s =====\n", msg);  
}

int main(void) {
    // Show basic process/arch info to catch 32/64-bit mismatches

    // Make sure DEBUG_MALLOC is on (okay if you also export in shell)
    setenv("DEBUG_MALLOC", "1", 1);

    // 1) Prove libpp is linked & stdout is visible
    banner("libpp check");
    pp(stdout, "pp alive from main() — hello!\n");
    fflush(stdout);

    // 2) Simple malloc → write → free
    banner("malloc/free");
    void *p = malloc(24);
    if (!p) {
        pp(stdout, "malloc failed at line 28\n");
        return 1;
    }
    memset(p, 0xAB, 24);
    free(p);

    // 3) realloc(NULL, n) should behave like malloc
    banner("realloc(NULL, n)");
    void *r = realloc(NULL, 64);
    if (!r) {
        pp(stdout, "realloc failed\n");
    }
    memset(r, 0xCD, 64);

    // 4) shrink-in-place: realloc down
    banner("realloc shrink");
    r = realloc(r, 16);   // should not move; your logger will show
    if (!r) {
        pp(stdout, "realloc 16 failed\n");
    }

    // 5) grow: may expand-in-place or move; either way, log shows it
    banner("realloc grow");
    void *oldr = r;
    r = realloc(r, 2000);
    if (!r) {
        pp(stdout, "Realloc 2000 failed\n");
    }
    if (r != oldr)
        pp(stdout, "realloc moved block\n");

    free(r);

    // 6) calloc (tests zeroing and overflow path)
    banner("calloc");
    void *c = calloc(3, 10);  // 30 bytes
    if (!c) { 
        pp(stdout, "calloc(3,10) failed\n"); 
        return 1; 
    }
    // sanity check: data should be zero
    for (size_t i = 0; i < 30; i++) {
        if (((unsigned char*)c)[i] != 0) {
            char buf[128];
            snprintf(buf, sizeof(buf), 
                    "ERROR: calloc result not zeroed at i=%zu\n", i);
            pp(stdout, buf);
            break;
        }
    }
    free(c);

    // 7) free(NULL) must be a no-op (but you log it)
    banner("free(NULL)");
    free(NULL);

    // 8) zero-size policy (your code returns NULL — verify)
    banner("zero-size policy");
    void *z1 = malloc(0);
    void *z2 = calloc(0, 16);
    void *z3 = calloc(16, 0);

    // Done
    banner("done");
    return 0;
}
