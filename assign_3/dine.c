#include "dine.h"
#include "dawdle.h"
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef NUM_PHILOSOPHERS
#define NUM_PHILOSOPHERS 5
#endif

int philosopher_id = 0;
sem_t *semaphore;
sem_t *forks;
philosopher_t *philosophers;

void *philosopher_body(void *arg);
void print_header();
void print_status();
/**
 * @brief
 *
 * Allocate sizes for the threads, semaphores, and the philsopher struct
 */
int main(int argc, char **argv) {
  int num_cycles = 1;
  if (argc == 2) {
    num_cycles = atoi(argv[1]);
    if (num_cycles <= 0)
      num_cycles = 1;
  }

  // Now allocate the threads, semaphore, forks, philsophers
  dawdle();
  philosophers = malloc(NUM_PHILOSOPHERS * sizeof(philosopher_t));
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

    philosophers[i].has_right = false;
    philosophers[i].has_left = false;

    snprintf(philosophers[i].name, sizeof(philosophers[i].name), "%c", 'A' + i);
  }
  print_header();
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
    // Check if even
    bool even = (p->id % 2 == 0);

    // check which fork is first
    int first = even ? right : left;
    int second = even ? left : right;

    // Pick up first fork
    sem_wait(&forks[first]);

    sem_wait(semaphore);
    if (first == left) {
      p->has_left = true;
    } else if (first == right) {
      p->has_right = true;
    }
    print_status();
    sem_post(semaphore);

    // Pick up second fork
    sem_wait(&forks[second]);
    sem_wait(semaphore);
    if (second == left) {
      p->has_left = true;
    } else if (second == right) {
      p->has_right = true;
    }
    print_status();
    sem_post(semaphore);

    // Eat now!
    sem_wait(semaphore);
    assert(p->has_left && p->has_right);
    p->state = EATING;
    print_status();
    sem_post(semaphore);
    dawdle();

    // Now in the changing state
    sem_wait(semaphore);
    p->state = CHANGING;
    print_status();
    sem_post(semaphore);

    // Now that we have eaten, we put down forks one at a time
    sem_wait(semaphore);
    if (second == left) {
      p->has_left = false;
    } else if (second == right) {
      p->has_right = false;
    }
    sem_post(&forks[second]);
    print_status();
    sem_post(semaphore);

    // Put down first fork
    sem_wait(semaphore);
    if (first == left) {
      p->has_left = false;
    } else if (first == right) {
      p->has_right = false;
    }

    sem_post(&forks[first]);
    print_status();
    sem_post(semaphore);

    // Think and mark thinking
    sem_wait(semaphore);
    assert(!(p->has_right) && !(p->has_left)); // ensure no forks
    p->state = THINKING;
    print_status();
    sem_post(semaphore);
    dawdle();

    sem_wait(semaphore);
    p->state = CHANGING;
    print_status();
    sem_post(semaphore);
  }
  return NULL;
}

/**
 * Loop through each of the philsophers, ensure
 * print on a status change and what forks they
 * are holding on to
 *
 */
void print_status()
{
  for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
    printf("| ");
    for (int j = 0; j < NUM_PHILOSOPHERS; j++) {
      if (philosophers[i].has_left && philosophers[i].fork_left == j) {
        printf("%d", j);
      } else if (philosophers[i].has_right && philosophers[i].fork_right == j) {
        printf("%d", j);
      } else {
        printf("-");
      }
    }

    switch (philosophers[i].state) {
    case CHANGING:
      printf("       ");
      break;
    case THINKING:
      printf(" Think ");
      break;
    case EATING:
      printf(" Eat   ");
      break;
    }
  }
  printf("|\n");
}


#define CELL_WIDTH 13   // Adjust as needed for column width (13 is good visually)

void print_header(void) {
  // Top border
  for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
    printf("|");
    for (int j = 0; j < CELL_WIDTH; j++) putchar('=');
  }
  printf("|\n");

  // Header row with philosopher names
  for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
    printf("|");
    int padding = (CELL_WIDTH - 1) / 2;
    for (int j = 0; j < padding - 1; j++) putchar(' ');
    printf("%c", 'A' + i);
    for (int j = 0; j < CELL_WIDTH - padding; j++) putchar(' ');
  }
  printf("|\n");

  // Bottom border
  for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
    printf("|");
    for (int j = 0; j < CELL_WIDTH; j++) putchar('=');
  }
  printf("|\n");

  // Print initial table snapshot
  print_status();
}
