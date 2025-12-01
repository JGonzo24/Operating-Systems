// tests/t_wait.c
#include "lwp.h"
#include <stdio.h>
#include <stdint.h>

int worker(void* arg) {
    int id = (int)(uintptr_t)arg;
    printf("[tid=%d] worker%d running\n", (int)lwp_gettid(), id);
    fflush(stdout);
    lwp_yield();
    printf("[tid=%d] worker%d exiting with %d\n", 
           (int)lwp_gettid(), id, 40 + id);
    fflush(stdout);
    lwp_exit(40 + id);  // Remove the void* cast - just pass int
    return 0;  // Won't reach here but good practice
}

int main(void){
  /* start the LWP system and create two workers */
  lwp_start();

  tid_t t1 = lwp_create(worker, (void*)1, 0);
  tid_t t2 = lwp_create(worker, (void*)2, 0);
  printf("[main] created tids: %d and %d\n", (int)t1, (int)t2);

  /* reap exactly two threads; lwp_wait returns the tid of a zombie */
  for(int i = 0; i < 2; ++i){
    int status = 0;
    tid_t reaped = lwp_wait(&status);
    if(reaped == NO_THREAD){
      printf("[reaper] nothing to reap (unexpected)\n");
      break;
    }
    printf("[reaper] reaped tid=%d raw_status=%d\n", (int)reaped, status);
  }

  return 0;
}


