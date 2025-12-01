#include "lwp.h"
#include <stdio.h>
#include <stdint.h>

int worker(void* arg) {
    int id = (int)(uintptr_t)arg;
    for(int i=0; i<4; i++){
        printf("[tid=%d] worker%d i=%d\n", (int)lwp_gettid(), id, i);
        fflush(stdout);
        lwp_yield();
    }
    return id & 0xFF;  // Return int, not void*
}

int main(void){
  setbuf(stdout, NULL);          // unbuffer stdout
  lwp_create(worker, (void*)(uintptr_t)1, 0);
  lwp_create(worker, (void*)(uintptr_t)2, 0);
  lwp_start();                   // should not return
  return 0;
}
