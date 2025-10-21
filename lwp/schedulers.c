#include "lwp.h"
#include "schedulers.h"
#include <stdio.h>

#define T_NEXT(t) ((t)->sched_one) // Macro to get the next thread pointer

typedef struct{
    thread head;
    thread tail;
    size_t num_threads;
} thread_pool;

struct scheduler rr_vtable = {
    .init = init,
    .shutdown = shutdown,
    .admit = admit,
    .remove = pool_remove,
    .next = next,
    .qlen = qlen
};

// Global vars
static thread_pool pool;

/**
 * @brief Inits the thread pool 
 * Can be called with NULL, handled in library file
 */
void init(void)
{
    pool.head = NULL;
    pool.tail = NULL;
    pool.num_threads = 0;
}

/**
 * @brief Tear down any structs
 */
void shutdown(void)
{
    pool.head = pool.tail = NULL;
    pool.num_threads = 0;
}

/**
 * @brief Admits a new thread to the thread pool
 * @param new thread to add into the pool
 * @return void
 */
void admit(thread new_thread)
{
    if (!new_thread)
    {
        return;
    }
    
    if (LWPSTATE(new_thread->status) != LWP_LIVE) 
    {
        return;   // don't enqueue dead threads
    }
    
    // Add to the end of the list
    T_NEXT(new_thread) = NULL;
    if (pool.tail) 
    {
        T_NEXT(pool.tail) = new_thread;
    }
    else
    {
        pool.head = new_thread;
    }           
    pool.tail = new_thread;
    pool.num_threads++;
}

/**
 * @brief Removes specific thread from round robin pool
 * 
 * Searches the pool's linked list for the vimtim thread and 
 * unlinks it, updating head/tail pointers as needed.
 * 
 * @param victim The thread to remove from the scheduling pool
 */
void pool_remove(thread victim)
{
    if (victim == NULL || !pool.head)
    {
        return;
    }

    thread prev = NULL;
    thread curr = pool.head;

    // Walk the list to find the victimk
    while(curr)
    {
        if (curr == victim)
        {
            // Unlink from the list
            if (prev)
            {
                T_NEXT(prev) = T_NEXT(curr);
            }
            else
            {
                pool.head = T_NEXT(curr); // Victim was head
            }
            
            // Update tail if we removed the last node
            if (pool.tail == curr) 
            {
                pool.tail = prev;
            }

            // Clean up and decrement count
            T_NEXT(curr) = NULL;
            pool.num_threads--;
            return;
        }
        prev = curr;
        curr = T_NEXT(curr);
    }
}

/**
 * @brief Selects the next thread to run in the scheduler
 * 
 * Returns the next thread at the head of the queue, then rotates it
 * to the back for the next scheduling roung. Automatically
 * removes any dead threads encountered at the head as well.
 * 
 * @return The next thread to schedule, or NULL if pool is empty
 */
thread next(void) {
    if (!pool.head)
    {
        return NULL;
    }

    // Clean up dead threads 
    while (pool.head && LWPSTATE(pool.head->status) != LWP_LIVE)
    {
        pool_remove(pool.head);
    }

    if (!pool.head) 
    {
        return NULL;
    }

    // Rotate: move the head to the back
    thread chosen = pool.head;   
    if (pool.head != pool.tail)
    {     // rotate AFTER choosing
        thread old = pool.head;
        pool.head = T_NEXT(old);
        T_NEXT(old) = NULL;
        T_NEXT(pool.tail) = old;
        pool.tail = old;
    }
    return chosen;
}

int qlen (void)
{
    return (int)pool.num_threads;
}
