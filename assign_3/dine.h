#ifndef DINE_H
#define DINE_H

#include <stdbool.h>
#include <pthread.h>
typedef enum { EATING = 0, THINKING = 1, CHANGING = 2 } state_t;

typedef struct philospher_t {
  state_t state;
  int id;
  char name[8];
  int fork_left;
  int fork_right;
  int cycles;
  pthread_t thread;
} philosopher_t;

void display_status(philosopher_t philospher);


#endif // DINE_H
