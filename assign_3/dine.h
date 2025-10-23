#ifndef DINE_H
#define DINE_H

#include <stdbool.h>
#include <pthread.h>
#include <assert.h>
#include <errno.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum { EATING = 0, THINKING = 1, CHANGING = 2 } state_t;

typedef struct philospher_t {
  state_t state;
  int id;
  char name[8];
  int fork_left;
  int fork_right;
  int cycles;
  pthread_t thread;

  bool has_left;
  bool has_right;
} philosopher_t;

void safe_wait(sem_t* semaphore);
void safe_post(sem_t* semaphore);
void print_header(void);
void print_status();
void *philosopher_body(void *arg);
void safe_alloc(void);
void safe_create(void);
void cleanup(void);
void init_all(int num_cycles);

void set_state_and_log(philosopher_t *p, state_t s);
void set_fork_flag_and_log(philosopher_t *p, int fork, bool has);
void pick_up(philosopher_t *p, int fork_index);
void put_down(philosopher_t *p, int fork_index);


#endif // DINE_H
