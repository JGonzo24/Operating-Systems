#include "dine.h"
#include "dawdle.h"
#include <semaphore.h>
#include <stdlib.h>

#ifndef NUM_PHILOSOPHERS
#define NUM_PHILOSOPHERS 5
#endif

// PADDING = 5 (for the width of 'think') + three spaces
#define PADDING 8
#define CELL_WIDTH NUM_PHILOSOPHERS + PADDING

sem_t *print_semaphore;
sem_t *forks;
philosopher_t *philosophers;

/**
 * @brief Inits, Creates, Free
 */
int main(int argc, char **argv) 
{
  // Take in arguments, default num_cycles to 1
  int num_cycles = 1;
  if (argc == 2) {
    num_cycles = atoi(argv[1]);
    if (num_cycles <= 0)
      num_cycles = 1;
  }

  // Allocate the philosophers and their forks
  safe_alloc();

  // Initialize philosophers and their forks 
  init_all(num_cycles);

  // Print out the first header for all philosopehrs
  print_header();

  // Create the threads
  safe_create();

  // Wait for threads to terminate, join
  cleanup();
}


/**
 * @brief A philosopher's meaning of life 
 */ 

void *philosopher_body(void *arg) 
{
  philosopher_t *p = arg;
  for (int i = 0; i < p->cycles; i++) {
    int left = p->fork_left;
    int right = p->fork_right;
    // Check if even
    bool even = (p->id % 2 == 0);

    // check which fork is first
    int first = even ? right : left;
    int second = even ? left : right;

    // Pick up forks in deadlock-avoiding order
    pick_up(p, first);
    pick_up(p, second);

    // Eat
    set_state_and_log(p, EATING);
    dawdle();

    // Transition
    set_state_and_log(p, CHANGING);

    // Put down forks
    put_down(p, second);
    put_down(p, first);

    // Think
    set_state_and_log(p, THINKING);
    dawdle();

    // Back to transitional state for next cycle
    set_state_and_log(p, CHANGING);

  }
  return NULL;
}


/**
 * Loop through each of the philsophers, ensure
 * print on a status change and what forks they
 * are holding on to.
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

void print_header(void) 
{
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

/**
  * @brief Error checking sem_wait()
  */
void safe_wait(sem_t* semaphore)
{
  int ret_val = sem_wait(semaphore);
  if (ret_val != 0)
  {
    perror("Problem with sem_wait()");
    exit(EXIT_FAILURE);
  }
}

/**
  * @brief Error checking sem_post();
  */  
void safe_post(sem_t* semaphore)
{
  int ret_val = sem_post(semaphore);
  if (ret_val != 0)
  {
    perror("Problem with sem_post()");
    exit(EXIT_FAILURE);
  }
}

/**
  * @brief Error checking and initializing semaphores and threads
  */  
void init_all(int num_cycles)
{

  // Init the semaphores (forks)
  for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
    int return_val = sem_init(&forks[i], 0, 1); 
    if (return_val != 0)
    {
      perror("sem_init() error!");
      exit(EXIT_FAILURE);
    }
  }
  int return_val = sem_init(print_semaphore, 0, 1); 
  if (return_val != 0)
  {
    perror("sem_init() error!");
    exit(EXIT_FAILURE);
  }


  // Init the philosophers
  for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
    philosophers[i].id = i;
    philosophers[i].state = CHANGING;
    philosophers[i].cycles = num_cycles;

    // Ensure the forks are wrapping
    philosophers[i].fork_left = i;
    philosophers[i].fork_right = (i + 1) % NUM_PHILOSOPHERS;

    // Ensure starting off with no held forks
    philosophers[i].has_right = false;
    philosophers[i].has_left = false;

    // Save name 
    snprintf(philosophers[i].name, sizeof(philosophers[i].name),
                                                         "%c", 'A' + i);
  }
}

/**
  * @brief Error checking malloc()
  */  
void safe_alloc(void)
{
  philosophers = malloc(NUM_PHILOSOPHERS * sizeof(philosopher_t));
  print_semaphore = malloc(sizeof(sem_t));
  forks = malloc(NUM_PHILOSOPHERS * sizeof(sem_t));

  if (!philosophers || !print_semaphore || !forks) {
    perror("Malloc failed!");
    exit(EXIT_FAILURE);
  }
}

/**
  * @brief Error checking pthread_create()
  */  
void safe_create(void)
{
  for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
    int return_val = pthread_create(&philosophers[i].thread, NULL,
                                    philosopher_body, &philosophers[i]);
    if (return_val != 0) {
      errno = return_val;
      perror("Thread create error");
      free(philosophers);
      free(print_semaphore);
      free(forks);

      exit(EXIT_FAILURE);
    }
  }
}

/**
  * @brief Cleans up and deallocates memory
  */  
void cleanup(void)
{
  for (int i = 0; i < NUM_PHILOSOPHERS; i++) {
    pthread_join(philosophers[i].thread, NULL);
  }

  // Destroy Semaphores
  for(int i = 0; i < NUM_PHILOSOPHERS; i++)
  {
    sem_destroy(&forks[i]);
  }
  sem_destroy(print_semaphore);

  free(forks);
  free(print_semaphore);
  free(philosophers);
}

/**
  * @brief Sets the new philosopher state and logs change
  */  
void set_state_and_log(philosopher_t *p, state_t s)
{
  safe_wait(print_semaphore);
  p->state = s;
  print_status();
  safe_post(print_semaphore);
}

/**
  * @brief Updates philosopher's has_fork boolean
  */  
void set_fork_flag_and_log(philosopher_t *p, int fork, bool has)
{
  safe_wait(print_semaphore);
  if (fork == p->fork_left) {
      p->has_left = has;
    } else if (fork == p->fork_right) {
      p->has_right = has;
    }
  print_status();
  safe_post(print_semaphore);
}

/**
  * @brief Philsopher picks up fork, updates flags, logs
  */ 
void pick_up(philosopher_t *p, int fork_index)
{
  safe_wait(&forks[fork_index]);
  set_fork_flag_and_log(p, fork_index, true);
}


/**
  * @brief Philsopher sets down fork, updates flags, logs
  */ 
void put_down(philosopher_t *p, int fork_index)
{
  set_fork_flag_and_log(p, fork_index, false);
  safe_post(&forks[fork_index]);
}
