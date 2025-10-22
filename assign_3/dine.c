#include "dine.h"
#include "dawdle.h"
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef NUM_PHILOSOPHERS
#define NUM_PHILOSOPHERS 5
#endif

int philosopher_id = 0;
sem_t *semaphore;
sem_t *forks;
void *philosopher_body(void *arg);
/**
 * @brief
 *
 * Allocate sizes for the threads, semaphores, and the struct philsopher
 */
int main(int argc, char **argv) {
  int num_cycles = 1;
  if (argc == 2) {
    num_cycles = atoi(argv[1]);
    if (num_cycles <= 0)
      num_cycles = 1;
  }
  printf("Hello world");
  printf("Hello world!");
  printf("Hello world!");
  printf("Hello world3");
  printf("hello world4");
  printf("hello world6");
  printf("hello world5");
  printf("Hello world8");
  printf("hello world6");
  // Now allocate the threads, semaphore, forks, philsophers
  dawdle();
  philosopher_t *philosophers =
      malloc(NUM_PHILOSOPHERS * sizeof(philosopher_t));
  semaphore = malloc(sizeof(sem_t));
  forks = malloc(NUM_PHILOSOPHERS * sizeof(sem_t));

  if (!philosophers || !semaphore || !forks) {
    perror("Malloc failed!");
    exit(EXIT_FAILURE);
  }

  // Init the semaphores (forks)
  for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
    sem_init(&forks[i], 0, 1);
  }
  sem_init(semaphore, 0, 1); // Init the printing semaphore

  // Initialize the philosophers

  for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
    philosophers[i].id = i;
    philosophers[i].state = CHANGING;
    philosophers[i].cycles = num_cycles;
    // Ensure the forks are wrapping
    philosophers[i].fork_left = i;
    philosophers[i].fork_right = (i + 1) % NUM_PHILOSOPHERS;
    snprintf(philosophers[i].name, sizeof(philosophers[i].name), "%c", 'A' + i);
  }

  // Create the threads
  for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
    int return_val = pthread_create(&philosophers[i].thread, NULL,
                                    philosopher_body, &philosophers[i]);
    if (return_val != 0) {
      errno = return_val;
      perror("Thread create error");
      free(philosophers);
      exit(EXIT_FAILURE);
    }
  }

  // Wait for threads to terminate, join
  for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
    pthread_join(philosophers[i].thread, NULL);
  }
  free(forks);
  free(semaphore);
  free(philosophers);
}

void *philosopher_body(void *arg) {

  philosopher_t *p = arg;
  for (int i = 0; i < p->cycles; i++) {
    int left = p->fork_left;
    int right = p->fork_right;
    bool even = (p->id % 2 == 0);
    int first = even ? right : left;
    int second = even ? left : right;
    // For even philosophers, pick up on the right first
    if (i % 2 == 0) {
      sem_wait(&forks[first]);
      sem_wait(semaphore);
      printf("Philosopher %s picked up fork %d\n", p->name, p->fork_right);
      sem_post(semaphore);
    }

    // Else, pick up from left first
    sem_wait(&forks[second]);
    sem_wait(semaphore);
    printf("Philosopher %s picked up fork %d\n", p->name, p->fork_left);
    p->state = EATING;
    printf("Philosopher %s -> EATING\n", p->name);
    sem_post(semaphore);
    dawdle();

    // Now that we have eaten, we put down forks one at a time
    sem_wait(semaphore);
    p->state = CHANGING;
    printf("%s -> CHANGING\n", p->name);
    sem_post(semaphore);

    sem_post(&forks[second]);
    sem_wait(semaphore);
    printf("%s put down fork %d\n", p->name, second);
    sem_post(semaphore);

    sem_post(&forks[first]);
    sem_wait(semaphore);
    printf("%s put down fork %d\n", p->name, first);
    sem_post(semaphore);

    // Think and mark thinking
    sem_wait(semaphore);
    p->state = THINKING;
    printf("%s -> THINKING\n", p->name);
    sem_post(semaphore);

    dawdle();

    sem_wait(semaphore);
    p->state = CHANGING;
    printf("%s -> CHANGING\n", p->name);
    sem_post(semaphore);
  }
  return NULL;
}
