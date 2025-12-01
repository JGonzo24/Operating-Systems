#include "lwp.h"
#include "sched_rr.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stddef.h>

/* ---- Helpers ----*/
static size_t round_up(size_t x, size_t a){ return (x + a-1) & ~(a-1); }

static size_t default_stack_size(void){
  struct rlimit rl;
  long pg = sysconf(_SC_PAGESIZE);
  size_t sz = (getrlimit(RLIMIT_STACK,&rl)==0 && rl.rlim_cur>0) ? 
					rl.rlim_cur : (8ul<<20);
  return round_up(sz, (size_t)pg);
}

void *lwp_stack_alloc(size_t *sz_out){
  size_t sz = default_stack_size();
  void *p = mmap(NULL, sz, PROT_READ|PROT_WRITE,
		 MAP_PRIVATE|MAP_ANONYMOUS|MAP_STACK, -1, 0);
  if(p == MAP_FAILED) return NULL;
  if(sz_out) *sz_out = sz;
  return p;
}

void lwp_stack_free(void *base, size_t sz){
  if(base) munmap(base, sz);
}

/* ---- External assembly context switch ---- */
extern void swap_rfiles(rfile *old, rfile *new);

/* ---- Global state management ---- */
extern struct scheduler rr_publish;

/* Thread queues */
static thread blocked_threads_head = NULL;
static thread blocked_threads_tail = NULL;
static thread global_thread_list_head = NULL;
static thread global_thread_list_tail = NULL;
static thread zombie_queue_head = NULL;
static thread zombie_queue_tail = NULL;

/* System state */
static tid_t thread_id_counter = 1;
static scheduler active_scheduler = NULL;
static scheduler fallback_scheduler = &rr_publish;
static thread running_thread = NULL;
static int lwp_system_active = 0;

/* Configuration constants */
#define DEFAULT_STACK_SIZE (8 * 1024 * 1024)  /* 8MB */

/* ---- Queue management utilities ---- */

/**
 *  * Enqueue a thread to the end of a FIFO queue
 *   */
static void enqueue_thread(thread *queue_head, thread *queue_tail, thread t) {
    t->lib_two = NULL;
    
    if (*queue_tail == NULL) {
        *queue_head = t;
        *queue_tail = t;
    } else {
        (*queue_tail)->lib_two = t;
        *queue_tail = t;
    }
}

/**
 *  * Dequeue a thread from the front of a FIFO queue
 *   */
static thread dequeue_thread(thread *queue_head, thread *queue_tail) {
    if (*queue_head == NULL) {
        return NULL;
    }
    
    thread removed = *queue_head;
    *queue_head = removed->lib_two;
    
    if (*queue_head == NULL) {
        *queue_tail = NULL;
    }
    
    removed->lib_two = NULL;
    return removed;
}

/**
 *  * Add thread to global tracking list
 *   */
static void register_thread(thread *list_head, thread *list_tail, thread t) {
    t->lib_one = NULL;
    
    if (*list_head == NULL) {
        *list_head = t;
        *list_tail = t;
    } else {
        (*list_tail)->lib_one = t;
        *list_tail = t;
    }
}

/**
 *  * Remove thread from global tracking list
 *   */
static void unregister_thread(thread *list_head, thread *list_tail, thread t) {
    if (*list_head == NULL || t == NULL) {
        return;
    }

    /* Special case: removing head */
    if (*list_head == t) {
        *list_head = t->lib_one;
        if (*list_tail == t) {
            *list_tail = NULL;
        }
        t->lib_one = NULL;
        return;
    }
    
    /* Search and remove */
    thread previous = *list_head;
    thread current = (*list_head)->lib_one;

    while (current != NULL) {
        if (current == t) {
            previous->lib_one = current->lib_one;
            if (*list_tail == t) {
                *list_tail = previous;
            }
            t->lib_one = NULL;
            return;
        }
        previous = current;
        current = current->lib_one;
    }
}

/* ---- System configuration helpers ---- */

/**
 *  * Get system page size for memory alignment
 *   */
static long fetch_page_size(void) {
    return sysconf(_SC_PAGE_SIZE);
}

/**
 *  * Initialize scheduler if not already set
 *   */
static void ensure_scheduler_ready(void) {
    if (fallback_scheduler == NULL) {
        fallback_scheduler = &rr_publish;
    }
    
    if (active_scheduler == NULL) {
        active_scheduler = fallback_scheduler;
        if (active_scheduler->init) {
            active_scheduler->init();
        }
    }
}

/**
 *  * Calculate appropriate stack size based on system limits
 *   */
static unsigned long calculate_stack_size(void) {
    struct rlimit resource_limits;
    size_t computed_size = DEFAULT_STACK_SIZE;
    
    if (getrlimit(RLIMIT_STACK, &resource_limits) == 0) {
        if (resource_limits.rlim_cur != RLIM_INFINITY && 
            resource_limits.rlim_cur != 0) {
            computed_size = (size_t)resource_limits.rlim_cur;
        }
    } else {
        perror("getrlimit() failed: using default 8MB stack");
    }

    /* Align to page boundary */
    long page_sz = fetch_page_size();
    if (computed_size % page_sz != 0) {
        computed_size += page_sz - (computed_size % page_sz);
    }

    return computed_size;
}

/**
 *  * Wrapper to call user function and handle exit
 *   */
static void thread_entry_wrapper(lwpfun user_function, void *user_arg) {
    int exit_code = user_function(user_arg);
    lwp_exit(exit_code);
}

/* ---- LWP API Implementation ---- */

tid_t lwp_create(lwpfun function, void *argument, size_t stackhint) {
    (void)stackhint;  /* Ignored per spec */
    
    size_t stack_sz = calculate_stack_size();
    void *stack_mem = mmap(NULL, stack_sz, PROT_READ | PROT_WRITE, 
                           MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    
    if (stack_mem == MAP_FAILED) {
        perror("mmap() failed to allocate thread stack");
        return NO_THREAD;
    }

    /* Allocate thread descriptor */
    thread new_lwp = (thread)malloc(sizeof(*new_lwp));
    if (new_lwp == NULL) {
        munmap(stack_mem, stack_sz);
        perror("malloc() failed to allocate thread descriptor");
        return NO_THREAD;
    }

    /* Initialize thread descriptor */
    memset(new_lwp, 0, sizeof(*new_lwp));
    new_lwp->stack = stack_mem;
    new_lwp->stacksize = stack_sz;
    new_lwp->tid = thread_id_counter++;
    new_lwp->status = MKTERMSTAT(LWP_LIVE, 0);
    memset(&new_lwp->state, 0, sizeof(new_lwp->state));
    new_lwp->state.fxsave = FPU_INIT;
    new_lwp->sched_two = NULL;
    new_lwp->sched_one = NULL;
    new_lwp->lib_one = NULL;
    new_lwp->lib_two = NULL;

    /* Set up initial stack frame for context switch */
    uintptr_t stack_top = ((uintptr_t)new_lwp->stack + new_lwp->stacksize)
							 & ~0xFUL;
    uintptr_t initial_frame = stack_top - 24;

    /* Create fake return frame */
    *(uint64_t*)(initial_frame + 0)  = 0;  /* saved base pointer */
    *(uint64_t*)(initial_frame + 8)=(uint64_t)(uintptr_t)thread_entry_wrapper;
    *(uint64_t*)(initial_frame + 16) = 0;  /* alignment padding */

    /* Initialize CPU registers */
    new_lwp->state.rbp = initial_frame;
    new_lwp->state.rsp = initial_frame;
    new_lwp->state.rdi = (uint64_t)(uintptr_t)function;  /* first arg */
    new_lwp->state.rsi = (uint64_t)(uintptr_t)argument;  /* second arg */

    /* Handle TID overflow */
    if (thread_id_counter == 0) {
        thread_id_counter = 1;
    }
    
    ensure_scheduler_ready();

    /* Register with system and scheduler */
    register_thread(&global_thread_list_head, 
		&global_thread_list_tail, new_lwp);
    
    if (active_scheduler && active_scheduler->admit) {
        active_scheduler->admit(new_lwp);
    }
    
    return new_lwp->tid;
}

/**
 *  * Start the LWP system
 *   */
void lwp_start(void) {
    if (lwp_system_active) {
        return;  /* Already started */
    }

    ensure_scheduler_ready();

    /* Convert calling thread into an LWP */
    thread original_thread = (thread)malloc(sizeof(*original_thread));
    if (original_thread == NULL) {
        perror("malloc() failed for original thread");
        exit(1);
    }
    
    memset(original_thread, 0, sizeof(*original_thread));
    
    running_thread = original_thread;
    original_thread->tid = thread_id_counter++;
    original_thread->status = MKTERMSTAT(LWP_LIVE, 0);
    memset(&original_thread->state, 0, sizeof(original_thread->state));
    original_thread->state.fxsave = FPU_INIT;
    original_thread->stack = NULL;  /* Uses existing system stack */
    original_thread->stacksize = 0;
    original_thread->lib_one = NULL;
    original_thread->lib_two = NULL;

    register_thread(&global_thread_list_head, 
			&global_thread_list_tail, original_thread);
    
    
    if (active_scheduler && active_scheduler->admit){
       active_scheduler->admit(original_thread);}
    lwp_system_active = 1;

    /* Save current context and begin scheduling */
    swap_rfiles(&original_thread->state, NULL);
    lwp_yield();
}

/**
 *  * Yield CPU to another thread
 *   */
void lwp_yield(void) {
    thread next_lwp = NULL;
    
    if (active_scheduler && active_scheduler->next) {
        next_lwp = active_scheduler->next();
    }

    /* No threads left - exit program */
    if (next_lwp == NULL) {
        exit(LWPTERMSTAT(running_thread ? running_thread->status : 0));
    }

    /* Same thread selected - just return */
    if (next_lwp == running_thread) {
        return;
    }

    /* Perform context switch */
    thread old_thread = running_thread;
    running_thread = next_lwp;
    swap_rfiles(&old_thread->state, &next_lwp->state);
}

/**
 *  * Terminate current thread
 *   */
void lwp_exit(int exit_status) {
    running_thread->status = MKTERMSTAT(LWP_TERM, exit_status);

    /* Remove from scheduler */
    if (active_scheduler && active_scheduler->remove) {
        active_scheduler->remove(running_thread);
    }

    /* Check for waiting thread */
    thread waiting_thread = dequeue_thread(&blocked_threads_head, 
						&blocked_threads_tail);
    
    if (waiting_thread != NULL) {
        /* Hand off directly to waiter */
        waiting_thread->sched_two = running_thread;
        if (active_scheduler && active_scheduler->admit) {
            active_scheduler->admit(waiting_thread);
        }
    } else {
        /* No waiter - add to zombie queue */
        enqueue_thread(&zombie_queue_head, &zombie_queue_tail, running_thread);
    }
    
    lwp_yield();  /* Never returns */
}

/**
 *  * Wait for a thread to terminate
 *   */
tid_t lwp_wait(int *exit_status) {
    /* Check for existing zombie */
    thread dead_thread = dequeue_thread(&zombie_queue_head, 
						&zombie_queue_tail);

    if (dead_thread != NULL) {
        /* Zombie available - reap immediately */
        tid_t dead_tid = dead_thread->tid;

        if (exit_status != NULL) {
            *exit_status = LWPTERMSTAT(dead_thread->status);
        }
        
        unregister_thread(&global_thread_list_head, 
				&global_thread_list_tail, dead_thread);

        if (dead_thread->stack != NULL && dead_thread->stacksize > 0) {
            munmap(dead_thread->stack, dead_thread->stacksize);
        }
        free(dead_thread);
        
        return dead_tid;
    }

    /* No zombie available - block until one exits */
    if (active_scheduler && active_scheduler->remove) {
        active_scheduler->remove(running_thread);
    }

    running_thread->sched_two = NULL;
    enqueue_thread(&blocked_threads_head, 
 			&blocked_threads_tail, running_thread);

    lwp_yield();  /* Block here */

    /* Resumed after a thread exited */
    dead_thread = running_thread->sched_two;
    
    if (dead_thread != NULL) {
        tid_t dead_tid = dead_thread->tid;
        
        if (exit_status != NULL) {
            *exit_status = LWPTERMSTAT(dead_thread->status);
        }

        unregister_thread(&global_thread_list_head, 
				&global_thread_list_tail, dead_thread);
        
        if (dead_thread->stack != NULL && dead_thread->stacksize > 0) {
            munmap(dead_thread->stack, dead_thread->stacksize);
        }
        free(dead_thread);
        running_thread->sched_two = NULL;

        return dead_tid;
    }
    
    return NO_THREAD;
}

/**
 *  * Get current thread ID
 *   */
tid_t lwp_gettid(void) {
    return running_thread ? running_thread->tid : NO_THREAD;
}

/**
 *  * Look up thread by ID
 *   */
thread tid2thread(tid_t tid) {
    thread iterator = global_thread_list_head;
    
    while (iterator != NULL) {
        if (iterator->tid == tid) {
            return iterator;
        }
        iterator = iterator->lib_one;
    }
    
    return NULL;
}

/**
 *  * Install a new scheduler
 *   */
void lwp_set_scheduler(scheduler new_sched) {
    if (new_sched == active_scheduler) {
        return;  /* Already using this scheduler */
    }

    scheduler previous_sched = active_scheduler;
    scheduler target_sched = new_sched ? new_sched : fallback_scheduler;

    if (target_sched == previous_sched) {
        return;
    }

    /* Initialize new scheduler */
    if (target_sched->init) {
        target_sched->init();
    }

    /* Migrate threads from old to new scheduler */
    if (previous_sched != NULL) {
        thread migrating_thread;
        
        while ((migrating_thread = previous_sched->next()) != NULL) {
            previous_sched->remove(migrating_thread);
            target_sched->admit(migrating_thread);
        }

        /* Shutdown old scheduler */
        if (previous_sched->shutdown) {
            previous_sched->shutdown();
        }
    }
    
    active_scheduler = target_sched;
}

/**
 *  * Get current scheduler
 *   */
scheduler lwp_get_scheduler(void) {
    return active_scheduler;
}
