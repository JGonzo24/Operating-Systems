// FILE: src/main.c
#include "lwp.h"
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

#ifndef MAX_NO_THREAD_SPINS
#define MAX_NO_THREAD_SPINS  1000  /* why: detect broken non-blocking wait() */
#endif

static int thread_func(void *arg) {
    int id = (int)(intptr_t)arg;
    printf("Thread %d: starting\n", id);
    printf("Thread %d: function entered, about to print\n", id);
    fflush(stdout);

    for (int i = 0; i < 3; i++) {
        printf("Thread %d: iteration %d\n", id, i);
        printf("Thread %d: about to yield\n", id);
        fflush(stdout);
        lwp_yield();
    }

    printf("Thread %d: done\n", id);
    fflush(stdout);
    return id; /* exit status is id (0..255 used in your lwp_exit) */
}

int main(void) {
    const int N = 3;
    tid_t tids[3] = {0};

    printf("Starting LWP test\n");
    fflush(stdout);

    lwp_start();

    printf("Creating %d threads\n", N);
    fflush(stdout);

    tids[0] = lwp_create(thread_func, (void*)(intptr_t)1);
    tids[1] = lwp_create(thread_func, (void*)(intptr_t)2);
    tids[2] = lwp_create(thread_func, (void*)(intptr_t)3);

    printf("Threads created: %lu, %lu, %lu\n",
           (unsigned long)tids[0],
           (unsigned long)tids[1],
           (unsigned long)tids[2]);
    fflush(stdout);

    /* Kick the scheduler once so someone runs before we wait. */
    lwp_yield();

    int completed = 0;
    int no_thread_spins = 0;

    while (completed < N) {
        int status = -1;
        tid_t exited = lwp_wait(&status);

        if (exited == NO_THREAD) {
            /* why: some buggy wait() impls return NO_THREAD early; nudge scheduler */
            if (++no_thread_spins % 50 == 0) {
                fprintf(stderr,
                        "[TEST] lwp_wait returned NO_THREAD (%d/%d done). "
                        "Nudging scheduler (spin=%d)\n",
                        completed, N, no_thread_spins);
            }
            if (no_thread_spins > MAX_NO_THREAD_SPINS) {
                fprintf(stderr,
                        "[TEST][FAIL] lwp_wait() kept returning NO_THREAD while %d/%d still pending.\n",
                        completed, N);
                return 2;
            }
            lwp_yield();
            continue;
        }

        no_thread_spins = 0; /* made progress */

        /* Validate status range (your lwp_exit packs to 0..255) */
        if (status < 0 || status > 255) {
            fprintf(stderr, "[TEST][WARN] Out-of-range status %d from tid=%lu\n",
                    status, (unsigned long)exited);
        }

        printf("Thread %lu exited with status %d\n",
               (unsigned long)exited, status);
        fflush(stdout);

        completed++;
    }

    printf("All threads completed (N=%d)\n", N);
    fflush(stdout);
    return 0;
}
