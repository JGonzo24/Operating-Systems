#include "lwp.h"
#include "schedulers.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

// --------- GLOBALS -------
extern struct scheduler rr_vtable;

static thread waiting_head = NULL;
static thread waiting_tail = NULL;


static thread all_list_head = NULL;
static thread all_list_tail = NULL;

static thread term_head = NULL;
static thread term_tail = NULL;

static tid_t next_tid = 1;
scheduler default_scheule = &rr_vtable;
scheduler curr_schedule = NULL;
static thread curr_thread = NULL;
static int system_started = 0;

#define STACK_SIZE 8*1024*1024 // 8MB default stack size
// --------- HELPER FUNCTIONS -------

/**
 * @brief Pushes thread onto corresponding stack
 */
static void fifo_push(thread *head, thread* tail, thread target)
{

    // Update the target's next pointer
    target->lib_two = NULL;
    if (*tail == NULL)
    {
        *head = *tail = target;
    }
    // Push to the end of the FIFO
    else
    {
        (*tail)->lib_two = target;
        *tail = target;
    }
}

/**
 * @brief Pops thread from corresponding stack
 */
static thread fifo_pop(thread *head, thread* tail)
{
    if (*head == NULL)
    {
        return NULL;
    }

    // Update the head's next pointer
    thread t = *head;
    *head = t->lib_two;
    if (*head == NULL)
    {
        *tail = NULL;
    }
    // Unlink the old head
    t->lib_two = NULL;
    return t;
}

/**
 * @brief Removes the thread from the all queue
 */
static void all_remove(thread *head, thread *tail, thread target)
{
    if (*head == NULL || target == NULL)
        return;

    // If the head is the target, update accordingly
    if (*head == target)
    {
        *head = target->lib_one;
        if (*tail == target)
        {
            *tail = NULL;
        }
        target->lib_one = NULL;
        return;
    }
    thread prev = *head;
    thread cur = (*head)->lib_one;

    // Iterate through the FIFO to remove correct thread
    while(cur)
    {
        if (cur == target)
        {
            // Update the prev->next to curr->next
            prev->lib_one = cur->lib_one;
            if (*tail == target)
            {
                *tail = prev;
            }
            target->lib_one = NULL;
            return;
        }
        // Keep iterating if not found
        prev = cur;
        cur = cur->lib_one;
    }
}

/**
 * @brief Add a thread to the global all thread FIFO
 */
static void all_add(thread *head, thread *tail, thread target)
{
    // If the start of the FIFO, update
    target->lib_one = NULL;
    if (*head == NULL)
    {
        *head = *tail = target;
    }

    // Add to the end of the FIFO
    else 
    {
        (*tail)->lib_one = target;
        *tail = target;
    }
}

/**
 * @brief Returns page size 
 * @return Page Size
 */
long get_page_size(void)
{
    return sysconf(_SC_PAGE_SIZE);
}

/**
 * @brief Returns the current schedule running
 */
static void get_sched(void)
{
    // Ensure there is a global default schedule
    if (!default_scheule)
    {
        default_scheule = &rr_vtable;
    }
    // If there is no current schedule, make it the default one
    if (!curr_schedule)
    {
        curr_schedule = default_scheule;
        if(curr_schedule->init)
        {
            curr_schedule->init();
        }
    }
}

/**
 * @brief Uses the stack limit to calculate the stack size 
 * Ensures that the stack is a mulitiple of the page size
 */
unsigned long get_stack_size(void)
{
    struct rlimit limits;
    size_t stack_size = STACK_SIZE; // default stack size
    if (getrlimit(RLIMIT_STACK, &limits) == 0)
    {
        if (limits.rlim_cur != RLIM_INFINITY && limits.rlim_cur != 0)
        {
            stack_size = (size_t)limits.rlim_cur;
        }
    }
    else
    {
        perror("getrlimit(): Setting stack to 8MB (default)");
    }

    // Round up to the next page size!
    long page_size = get_page_size();
    if (stack_size % page_size != 0)
    {
        stack_size += page_size - (stack_size % page_size);
    }
    return stack_size;
}

/**
 * @brief Wrapper function that is used for getting the exit status
 * of a ran function. 
 */
static void lwp_wrap(lwpfun fun, void *arg) { 
    int rval; 
    rval=fun(arg); 
    lwp_exit(rval); 
}

 // ------------ MAIN FUNCTIONS --------------

/**
 * @brief This function creates a new LWP
 * @param function The function the LWP executes
 * @param argument The arguments to the function
 * @return Returns the thread ID of the new thread
 */
tid_t lwp_create(lwpfun function, void *argument)
{
    // Allocate a stack and a context for each LWP
    size_t stack_size = get_stack_size();
    void *stack = mmap(NULL, stack_size, PROT_READ | PROT_WRITE, MAP_PRIVATE 
                                    | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    if (stack == MAP_FAILED)
    {
        perror("MMAP FAILED!");
        return NO_THREAD;
    }

    // init context
    thread new_thread = (thread)malloc(sizeof(*new_thread));
    if (!new_thread)
    {
        perror("MALLOC: FAILED");
        return NO_THREAD;
    }

    // Clear out the data given from malloc
    memset(new_thread, 0, sizeof(*new_thread));  
    
    // Initialize the allocated stack
    new_thread->stack = stack;
    new_thread->stacksize = stack_size;
    new_thread->tid = next_tid++;
    new_thread->status = MKTERMSTAT(LWP_LIVE, 0);
    new_thread->state.fxsave = FPU_INIT;
    new_thread->exited = NULL;
    new_thread->sched_one = NULL;

   /**
    * What the stack needs to look like on when it was called:
    * 1 word ret address of LWP_FUN() (can be anything)
    * 1 word LWP_WRAP() pointer (so it can be used when we pop rip)
    * 1 word for the base pointer (not used)
    */
    uintptr_t top_aligned = ((uintptr_t)new_thread->stack + 
                                    new_thread->stacksize) & ~0xFUL;
    uintptr_t frame_base = top_aligned - 24;

    // Fake frame: [rbp]=saved_rbp, [rbp+8]=retaddr
    *(uint64_t*)(frame_base)      = 0;      
    *(uint64_t*)(frame_base + 8)  = (uint64_t)(uintptr_t)lwp_wrap;  
    *(uint64_t*)(frame_base + 16) = 0;

    // Load context into the state registers
    new_thread->state.rbp = frame_base;
    new_thread->state.rsp = frame_base;           
    new_thread->state.rdi = (uint64_t)(uintptr_t)function;  // arg1 to lwp_wrap 
    new_thread->state.rsi = (uint64_t)(uintptr_t)argument;  // arg2 to lwp_wrap 

    // Don't allow for zero tid
    if (next_tid == 0)
    {
        next_tid = 1;
    }
    get_sched();

    // Init the next pointers
    new_thread->lib_one = NULL;
    new_thread->lib_two = NULL;

    // Add to global list of threads, add to current schedule
    all_add(&all_list_head, &all_list_tail, new_thread);

    if (curr_schedule && curr_schedule->admit)
    {
        curr_schedule->admit(new_thread);
    }
    return new_thread->tid;
} 

void lwp_start(void)
{
    if (system_started)
    {
        return;
    }

    get_sched();

    // Make a new context for the new thread
    thread main_thread = (thread)malloc(sizeof(*main_thread));
    if (!main_thread)
    {
        perror("malloc() main thread failed!");
        exit(1);
    }
    memset(main_thread, 0, sizeof(*main_thread));

    curr_thread = main_thread;
    main_thread->tid = next_tid++;
    main_thread->status = MKTERMSTAT(LWP_LIVE, 0);
    main_thread->state.fxsave = FPU_INIT;
    main_thread->stack = NULL;
    main_thread->stacksize = 0;

    main_thread->lib_one = NULL;
    main_thread->lib_two = NULL;

    all_add(&all_list_head, &all_list_tail, main_thread);
    if (curr_schedule && curr_schedule->admit)
    {
        curr_schedule->admit(main_thread);
    }
    system_started = 1;

    // Save initial state, yield() control
    swap_rfiles(&main_thread->state, NULL);
    lwp_yield();
}

/**
 * @brief Saves context, yields control to next thread
 */
void lwp_yield(void)
{
    // Get next thread
    thread next_thread = NULL;
    if (curr_schedule && curr_schedule->next)
    {
        next_thread = curr_schedule->next();
    }

    // Terminate the program if there are no more threads!
    if (next_thread == NULL)
    {
        exit(LWPTERMSTAT(curr_thread ? curr_thread->status : 0));
    }

    // If the next thread is the one you are currently running, return
    if (next_thread == curr_thread)
    {
        return;
    }

    // Update the current thread
    thread prev = curr_thread;
    curr_thread = next_thread;

    // Context switch
    swap_rfiles(&prev->state, &next_thread->state);
}

/**
 * @brief Terminates the current thread and hands it off to a waiting thread 
 * 
 * Marks the thread as terminated, removes itself from the scheduler, and:
 * - Either pairs with a waiting thread (waiter consumes immediately) or
 * - queues itself in a terminated list (to be consumed by lwp_wait() later)
 */
void lwp_exit(int exitval)
{
    // The exit status is in the low bytes of exitval
    curr_thread->status = MKTERMSTAT(LWP_TERM, exitval);

    // Remove from scheduler
    if (curr_schedule && curr_schedule->remove)
    {
        curr_schedule->remove(curr_thread);
    }

    // Check if any thread is blocked in lwp_wait()
    thread waiter = fifo_pop(&waiting_head, &waiting_tail);
    if (waiter)
    {
        // Hand off: waiter will consume when it resumes
        waiter->exited = curr_thread;
        if (curr_schedule && curr_schedule->admit)
        {
            curr_schedule->admit(waiter);
        }
    }
    else 
    {
        // No waiter yet, queue on terminated FIFO
        fifo_push(&term_head, &term_tail, curr_thread);
    }
    lwp_yield(); // Switch to enext thread (doesn't return)
}


/**
 * @brief Waits for any thread to terminate and reaps its resources
 * 
 * Returns if a terminated thread is available, if not then blocks
 * until a thread calls lwp_exit(). Terminated thread is removed from
 * all lists and resources are freed. 
 * 
 * @param status Pointer to store the exits status
 * @return Thread ID of the reaped thread, or NO_THREAD on fail
 */
tid_t lwp_wait(int *status)
{
    // Check if any thread already terminated
    thread dead = fifo_pop(&term_head, &term_tail);

    if (dead)
    {
        // Thread exited before calling wait, so reap
        tid_t id = dead->tid;

        // Set the status of the dead thread
        if (status)
        {
            *status = LWPTERMSTAT(dead->status);
        }
        
        // Unlink from global list
        all_remove(&all_list_head, &all_list_tail, dead);

        // free resources, don't unmap original
        if (dead->stack && dead->stacksize)
        {
            munmap(dead->stack, dead->stacksize);
        }
        free(dead);
        return id;
    }

    // Block: remove from scheduler, enqueue on waiting fifo, yield
    if (curr_schedule && curr_schedule->remove)
    {
        curr_schedule->remove(curr_thread);
    }

    curr_thread->exited = NULL;

    fifo_push(&waiting_head, &waiting_tail, curr_thread);

    lwp_yield(); // Block until lwp_exit() pairs with current thread

    // Current thread resumed, check what thread handed itself to us
    dead = curr_thread->exited;
    if (dead)
    {
        tid_t id = dead->tid;
        if (status)
        {
            *status = LWPTERMSTAT(dead->status);
        }

        // Cleanup: remove from global list and free resources
        all_remove(&all_list_head, &all_list_tail, dead);
        if (dead->stack && dead->stacksize)
        {
            munmap(dead->stack, dead->stacksize);
        }
        free(dead);
        curr_thread->exited = NULL;

        return id;
    }
    // Should not get here
    return NO_THREAD;
}

/**
 * @brief Returns current thread's thread ID
 */
tid_t lwp_gettid(void)
{
    return curr_thread ? curr_thread->tid : NO_THREAD;
}

/**
 * @brief Iterates through global thread list to find thread
 * @return Returns the thread from its thread ID
 */
thread tid2thread(tid_t tid)
{
    thread cur = all_list_head;
    while (cur)
    {
        if (cur->tid == tid)
            return cur;
        cur = cur->lib_one;
    }
    return NULL;
}

/**
 * @brief Going to set the scheduler
 * Inits() the scheduler if needed
 * Admits() old threads into the new thread pool
 * Shutsdown() if needed
 */ 
void lwp_set_scheduler(scheduler new_scheduler)
{
    if (new_scheduler == curr_schedule)
    {
        return;
    }

    scheduler old = curr_schedule;
    scheduler new = new_scheduler ? new_scheduler : default_scheule;

    if (new == old)
    {
        return;
    }

    // Init new scheduler (can be NULL)
    if (new->init)
    {
        new->init();
    }
    // Transfer all threads from old to new scheduler
    if (old)
    {
        thread t;
        while ((t = old->next()) != NULL)
        {
            old->remove(t);
            new->admit(t);
        }

        // Shutdown old scheduler (can be NULL)
        if (old->shutdown)
        {
            old->shutdown();
        }
    }
    curr_schedule = new;
}

scheduler lwp_get_scheduler(void)
{
    return curr_schedule;
}
