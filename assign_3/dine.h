#ifndef DINE_H
#define DINE_H

#include <stdbool.h>

typedef enum {
    EATING = 0,
    THINKING = 1,
    CHANGING = 2
} state_t;


typedef struct {
    state_t state;

    int fork_left;
    int fork_right;

    bool has_fork_left;
    bool has_fork_right;

    bool pick_from_right;

    int name;

    philospher_t next_phil;
    philospher_t prev_phil;

} philospher_t;

void display_status(philospher_t philospher);
void add_to_table(int num_philosophers);


// Doubley linked list data structure
void add_phil(philospher_t philosopher);
void remove_phil(int name);

void get_next(philospher_t philospher);
void get_prev(philospher_t philospher);


#endif // DINE_H