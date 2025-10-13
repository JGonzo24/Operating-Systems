#include "lwp.h"
#include "schedulers.h"
#include <stdio.h>



/* === RR-only tracing (no global flags needed) ========================== */
#ifndef RR_TRACE
#define RR_TRACE 0 /* set to 0 to silence RR logs */
#endif
#if RR_TRACE
  #define RRDBG(fmt, ...) fprintf(stderr, "[RR] " fmt "\n", ##__VA_ARGS__)
#else
  #define RRDBG(...) ((void)0)
#endif



#define T_NEXT(t) ((t)->sched_one)

typedef struct{
    /* data */
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
 */
void init(void)
{
    pool.head = NULL;
    pool.tail = NULL;
    pool.num_threads = 0;
}

/* Dump the queue state (first up to 16 nodes) */
static void rr_dump_pool(const char *tag) {
#if RR_TRACE
    RRDBG("%s: head=%p tail=%p n=%zu", tag ? tag : "pool",
          (void*)pool.head, (void*)pool.tail, pool.num_threads);
    size_t i = 0;
    for (thread it = pool.head; it && i < 16; it = T_NEXT(it), ++i) {
        RRDBG("  [%zu] t=%p tid=%lu status=0x%x live=%d next=%p",
              i, (void*)it, (unsigned long)it->tid, (unsigned)it->status,
              LWPSTATE(it->status) == LWP_LIVE, (void*)T_NEXT(it));
    }
#endif
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
void admit(thread t)
{
    if (!t)
    {
        RRDBG("admit: NULL thread (ignored)");
        return;
    }
    if (LWPSTATE(t->status) != LWP_LIVE) 
    {
        RRDBG("admit: REJECT tid=%lu status=0x%x (not LIVE)",
              (unsigned long)t->tid, (unsigned)t->status);
        return;   // don't enqueue dead threads
    }
    RRDBG("admit: tid=%lu size %zu->%zu",
        (unsigned long)t->tid, pool.num_threads, pool.num_threads + 1);
    
    T_NEXT(t) = NULL;
    if (pool.tail) T_NEXT(pool.tail) = t;
    else           pool.head = t;
    pool.tail = t;
    pool.num_threads++;

    rr_dump_pool("After admit");
}

/**
 * @bried Removes specific thread from pool
 * @param victim the thread to be removed
 */
void pool_remove(thread victim)
{
    if (victim == NULL || !pool.head)
    {
        RRDBG("remove: victim=%p head=%p (ignored)", (void*)victim, (void*)pool.head);

        return;
    }
    RRDBG("remove: tid=%lu", (unsigned long)victim->tid);


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
    RRDBG("remove: tid=%lu not found", (unsigned long)victim->tid);
}

thread next(void) {
    if (!pool.head) return NULL;

    while (pool.head && LWPSTATE(pool.head->status) != LWP_LIVE) {
        pool_remove(pool.head);
    }
    if (!pool.head) return NULL;

    thread chosen = pool.head;        // choose head
    if (pool.head != pool.tail) {     // rotate AFTER choosing
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
    RRDBG("qlen=%zu", pool.num_threads);
    return (int)pool.num_threads;
}
