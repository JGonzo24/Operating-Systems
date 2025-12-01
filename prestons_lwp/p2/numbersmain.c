#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "lwp.h"
#include "sched_rr.h"

#define MAXSNAKES  100
#define INITIALSTACK 2048

static void indentnum(void *num);

int main(int argc, char *argv[]){
  long i;

  printf("Launching LWPS\n");

  /* spawn a number of individual LWPs */
  for(i=1;i<=5;i++) {
    lwp_create((lwpfun)indentnum,(void*)i,INITIALSTACK);
  }

  lwp_start();

  /* reap threads */
  for(i=1;i<=5;i++) {
    int status,num;
    tid_t t;
    t = lwp_wait(&status);
    num = LWPTERMSTAT(status);
    printf("Thread %ld exited with status %d\n",t,num);
  }

  printf("Back from LWPS.\n");
  return 0;
}

static void indentnum(void *num) {
  long i;
  int howfar;

  howfar=(long)num;
  for(i=0;i<howfar;i++){
    printf("%*d\n",howfar*5,howfar);
    lwp_yield();
  }
  lwp_exit(i);
}
