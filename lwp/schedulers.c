#include "lwp.h"
#include "schedulers.h"
#include <stdio.h>

#define T_NEXT(t) ((t)->sched_one)

typedef struct{
    /* data */
    thread head;
    thread tail;
    size_t num_threads;
} thread_pool;

// Global vars
static thread_pool pool;

/**
 * @brief Inits the thread pool
 */
void init(void)
{
    pool.head = NULL;
    pool.tail = NULL;
    pool.num_threads = 0;
}

/**
 * @brief This is going to tear down any structs
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
        fprintf(stderr, "admit: NULL thread\n");
        return;
    }
    // New thread's tail is going to be null
    T_NEXT(new_thread) = NULL; 

    // Else, add the new thread!
    if (pool.tail)
    {
        T_NEXT(pool.tail) = new_thread;
    }
    else 
    {
        pool.head = new_thread;
    }
    // Make the tail the new thread, increment
    pool.tail = new_thread;
    pool.num_threads++;
}

/**
 * @bried Removes specific thread from pool
 * @param victim the thread to be removed
 */
void pool_remove(thread victim)
{
    if (victim == NULL || !pool.head)
    {
        return;
    }

    thread prev = NULL;
    thread curr = pool.head;

    while(curr)
    {
        if (curr == victim)
        {
            // unlink the prev and next
            if (prev)
            {
                T_NEXT(prev) = T_NEXT(curr);
            }
            else
            {
                pool.head = T_NEXT(curr);
            }
            // 
            if (pool.tail == curr) 
            {
                pool.tail = prev;
            }
            T_NEXT(curr) = NULL;
            pool.num_threads--;
            return;
        }
        prev = curr;
        curr = T_NEXT(curr);
    }
}

thread next(void)
{
    thread t = pool.head;
    if (!t)
    {
        return NULL;
    }

    if (pool.head == pool.tail)
    {
        return t;
    }

    // pop head
    pool.head = T_NEXT(t);
    T_NEXT(t) = NULL;

    // push the older head to tail
    T_NEXT(pool.tail) = t;
    pool.tail = t;

    return t;
}

int qlen (void)
{
    return (int)pool.num_threads;
}
