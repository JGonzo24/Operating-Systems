#include "lwp.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int worker(void* arg) {
    int id = (int)(uintptr_t)arg;
    printf("[tid=%d] worker%d starting\n", (int)lwp_gettid(), id);
    fflush(stdout);
    lwp_yield();
    printf("[tid=%d] worker%d exiting\n", (int)lwp_gettid(), id);
    fflush(stdout);
    return id & 0xFF;
}

int reaper(void* arg) {
    (void)arg;
    int status;
    tid_t t;
    
    while((t = lwp_wait(&status)) != NO_THREAD) {
        printf("[tid=%d] reaped tid=%lu status=%d\n", 
               (int)lwp_gettid(), (unsigned long)t, status);
        fflush(stdout);
    }
    return 0;
}

int main(void) {
    lwp_create(worker, (void*)1, 0);
    lwp_create(worker, (void*)2, 0);
    lwp_create(worker, (void*)3, 0);
    lwp_create(reaper, NULL, 0);
    
    lwp_start();
    
    return 0;
}
