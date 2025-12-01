#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ncurses.h>
#include <signal.h>
#include <sys/time.h>
#include "snakes.h"
#include "lwp.h"
#include "util.h"

#define MAXSNAKES  100
#define INITIALSTACK 8192

int main(int argc, char *argv[]){
  int i,cnt,err;
  snake s[MAXSNAKES];

  err = 0;

  install_handler(SIGINT, SIGINT_handler);
  install_handler(SIGQUIT,SIGQUIT_handler);

  start_windowing();

  /* Initialize Snakes */
  cnt = 0;
  s[cnt++] = new_snake( 8,30,10, E,1);
  s[cnt++] = new_snake(10,30,10, E,2);
  s[cnt++] = new_snake(12,30,10, E,3);
  s[cnt++] = new_snake( 8,50,10, W,4);
  s[cnt++] = new_snake(10,50,10, W,5);
  s[cnt++] = new_snake(12,50,10, W,6);
  s[cnt++] = new_snake( 4,40,10, S,7);

  draw_all_snakes();

  /* turn each snake loose as an individual LWP */
  for(i=0;i<cnt;i++) {
    s[i]->lw_pid = lwp_create((lwpfun)run_snake,(void*)(s+i),INITIALSTACK);
  }

  lwp_start();

  for(i=0;i<cnt;i++)
    lwp_wait(NULL);

  end_windowing();

  printf("Goodbye.\n");
  return err;
}
