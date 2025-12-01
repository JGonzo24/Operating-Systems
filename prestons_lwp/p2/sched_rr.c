#include "lwp.h"
#include "sched_rr.h"
#include <stdio.h>
#include <stddef.h>

/* Macro for accessing scheduler-private link */
#define NEXT_IN_QUEUE(t) ((t)->sched_one)

/* Round-robin queue state */
typedef struct {
    thread front;
    thread back;
    size_t count;
} rr_queue;

/* Global scheduler vtable */
struct scheduler rr_publish = {
    .init = NULL,
    .shutdown = NULL,
    .admit = NULL,
    .remove = NULL,
    .next = NULL
};

/* Round-robin queue instance */
static rr_queue ready_queue;

/**
 *  * Add thread to end of ready queue
 *   */
static void rr_enqueue(thread t) {
    if (t == NULL) {
        return;
    }
    
    NEXT_IN_QUEUE(t) = NULL;
    
    if (ready_queue.back != NULL) {
        NEXT_IN_QUEUE(ready_queue.back) = t;
        ready_queue.back = t;
    } else {
        ready_queue.front = t;
        ready_queue.back = t;
    }
    
    ready_queue.count++;
}

/**
 *  * Remove specific thread from ready queue
 *   */
static void rr_dequeue(thread target) {
    if (target == NULL || ready_queue.front == NULL) {
        return;
    }

    thread prev = NULL;
    thread curr = ready_queue.front;

    /* Search for target in queue */
    while (curr != NULL) {
        if (curr == target) {
            /* Found it - unlink */
            if (prev != NULL) {
                NEXT_IN_QUEUE(prev) = NEXT_IN_QUEUE(curr);
            } else {
                /* Removing front */
                ready_queue.front = NEXT_IN_QUEUE(curr);
            }
            
            /* Update back pointer if needed */
            if (ready_queue.back == curr) {
                ready_queue.back = prev;
            }

            NEXT_IN_QUEUE(curr) = NULL;
            ready_queue.count--;
            return;
        }
        
        prev = curr;
        curr = NEXT_IN_QUEUE(curr);
    }
}

/**
 *  * Select next thread (rotate queue)
 *   */
static thread rr_select_next(void) {
    if (ready_queue.front == NULL) {
        return NULL;
    }

    thread selected = ready_queue.front;
    
    /* Rotate queue if more than one thread */
    if (ready_queue.front != ready_queue.back) {
        thread rotating = ready_queue.front;
        ready_queue.front = NEXT_IN_QUEUE(rotating);
        NEXT_IN_QUEUE(rotating) = NULL;
        NEXT_IN_QUEUE(ready_queue.back) = rotating;
        ready_queue.back = rotating;
    }
    
    return selected;
}

/**
 *  * Initialize scheduler at load time
 *   */
void __attribute__((constructor)) initialize_round_robin(void) {
    ready_queue.front = NULL;
    ready_queue.back = NULL;
    ready_queue.count = 0;
    
    rr_publish.init = NULL;
    rr_publish.shutdown = NULL;
    rr_publish.admit = rr_enqueue;
    rr_publish.remove = rr_dequeue;
    rr_publish.next = rr_select_next;
}
