#include "lwp.h"
#include <stdio.h>
#include <stdint.h>

// Change both ping and pong:
int ping(void* arg) {
    (void)arg;
    for(int i=0; i<3; i++){
        printf("ping\n");
        fflush(stdout);
        lwp_yield();
    }
    return 0;  // Return int
}

int pong(void* arg) {
    (void)arg;
    for(int i=0; i<3; i++){
        printf("pong\n");
        fflush(stdout);
        lwp_yield();
    }
    return 0;  // Return int
}

int main(void){
  setbuf(stdout, NULL);
  lwp_create(ping, NULL, 0);
  lwp_create(pong, NULL, 0);
  lwp_start();
  return 0;
}
