#include "lwp.h"
#include "schedulers.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>

// --------- GLOBALS -------
extern struct scheduler rr_vtable;

static thread waiting_head = NULL;
static thread waiting_tail = NULL;

static thread running_head = NULL;
static thread running_tail = NULL;

static thread all_list_head = NULL;
static thread all_list_tail = NULL;

static thread term_head = NULL;
static thread term_tail = NULL;

static tid_t next_tid = 1;
scheduler default_scheule = &rr_vtable;
scheduler curr_schedule = NULL;
static thread curr_thread = NULL;
static int system_started = 0;


#include <inttypes.h>

#ifndef LWP_DEBUG
#define LWP_DEBUG 0
#endif

#if LWP_DEBUG
  #define LWPDBG(fmt, ...) fprintf(stderr,"[LWP] " fmt "\n", ##__VA_ARGS__)
#else
  #define LWPDBG(...) ((void)0)
#endif

static inline int is_user_lwp(thread t) {
  return t && t->stack && t->stacksize;   // created by lwp_create()
}

static inline void dbg_dump_thread(const char* tag, thread t) {
  if (!t) { LWPDBG("%s: (null)", tag); return; }
  LWPDBG("%s: t=%p tid=%lu status=0x%x stack=[%p..%p) rbp=%#lx rsp=%#lx lib_one=%p lib_two=%p sched_one=%p",
         tag, (void*)t, (unsigned long)t->tid, (unsigned)t->status,
         t->stack, (void*)((char*)t->stack + t->stacksize),
         (unsigned long)t->state.rbp, (unsigned long)t->state.rsp,
         (void*)t->lib_one, (void*)t->lib_two, (void*)t->sched_one);
}

static inline void dbg_check_retaddr(const char* tag, thread t, void *expected_wrap) {
  if (!is_user_lwp(t)) {
    LWPDBG("%s: (system thread) skip retaddr check", tag);
    return;
  }
  uintptr_t rbp = (uintptr_t)t->state.rbp;
  uintptr_t lo  = (uintptr_t)t->stack;
  uintptr_t hi  = lo + t->stacksize;

  if (rbp < lo || rbp + 16 > hi) {
    LWPDBG("%s: BAD RBP range: rbp=%#lx not in stack [%#lx..%#lx)", tag,
           (unsigned long)rbp, (unsigned long)lo, (unsigned long)hi);
    return;
  }
  unsigned long long *frame = (unsigned long long*)rbp;
  unsigned long long prev_rbp = frame[0];
  unsigned long long retaddr  = frame[1];

  LWPDBG("%s: frame @%p -> [rbp]=%#llx  [rbp+8]=%#llx  (expect ret=%p)",
         tag, (void*)rbp, prev_rbp, retaddr, expected_wrap);

  if ((void*)(uintptr_t)retaddr != expected_wrap) {
    LWPDBG("%s: *** BAD RETADDR *** tid=%lu rbp=%#lx ret=%#llx (expected %p)",
           tag, (unsigned long)t->tid, (unsigned long)rbp, retaddr, expected_wrap);
  }
}
//------------------------------------------------------------------------

//target->lib_one = NEXT in all FIFO
// --------- HELPER FUNCTIONS -------
static void fifo_push(thread *head, thread* tail, thread target)
{
    target->lib_two = NULL;
    if (*tail == NULL)
    {
        *head = *tail = target;
    }
    else
    {
        (*tail)->lib_two = target;
        *tail = target;
    }
}

static thread fifo_pop(thread *head, thread* tail)
{
    if (*head == NULL)
    {
        return NULL;
    }

    thread t = *head;
    *head = t->lib_two;
    if (*head == NULL)
        *tail = NULL;

    t->lib_two = NULL;

    return t;
}

static void all_remove(thread *head, thread *tail, thread target)
{
    if (*head == NULL || target == NULL)
        return;
    
    if (*head == target)
    {
        *head = target->lib_one;
        if (*tail == target)
            *tail = NULL;
        target->lib_one = NULL;
        return;
    }
    thread prev = *head;
    thread cur = (*head)->lib_one;

    while(cur)
    {
        if (cur == target)
        {
            prev->lib_one = cur->lib_one;
            if (*tail == target)
                *tail = prev;
            target->lib_one = NULL;
            return;
        }
        prev = cur;
        cur = cur->lib_one;
    }
}

static void all_add(thread *head, thread *tail, thread target)
{
    target->lib_one = NULL;
    if (*head == NULL)
        *head = *tail = target;
    else 
    {
        (*tail)->lib_one = target;
        *tail = target;
    }
}

long get_page_size(void)
{
    return sysconf(_SC_PAGE_SIZE);
}

static void get_sched(void)
{
    if (!default_scheule)
        default_scheule = &rr_vtable;
    if (!curr_schedule)
    {
        curr_schedule = default_scheule;
        if(curr_schedule->init)
            curr_schedule->init();
    }
}

unsigned long get_stack_size(void)
{
    struct rlimit limits;
    size_t stack_size = 8 * 1024 * 1024; // default stack size
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

static void lwp_wrap(lwpfun fun, void *arg) { 
    int rval; 
    rval=fun(arg); 
    lwp_exit(rval); 
}

 // ------------ MAIN FUNCTIONS --------------
tid_t lwp_create(lwpfun function, void *argument)
{
    // Allocate a stack and a context for each LWP
    size_t stack_size = get_stack_size();
    void *stack = mmap(NULL, stack_size, PROT_READ | PROT_WRITE, MAP_PRIVATE 
                                    | MAP_ANONYMOUS | MAP_STACK, -1, 0);
 
    if (stack == MAP_FAILED)
    {
        perror("MMAP FAILED!");
        return (tid_t)-1;
    }

        // Allocate a guard page right after the stack to prevent overlap with next stack
    void *guard = mmap((char*)stack + stack_size, get_page_size(), PROT_NONE, MAP_PRIVATE 
                                            | MAP_ANONYMOUS, -1, 0);

    if (guard == MAP_FAILED) {
        perror("MMAP GUARD FAILED!");
        munmap(stack, stack_size);
        return (tid_t)-1;
    }
    // init context
    thread new_thread = (thread)malloc(sizeof(*new_thread));
    if (!new_thread)
    {
        perror("MALLOC: FAILED");
        return (tid_t) -1;
    }
    memset(new_thread, 0, sizeof(*new_thread));  

    new_thread->state.rax = 0;
    new_thread->state.rbx = 0;
    new_thread->state.rcx = 0;
    new_thread->state.rdx = 0;
    // ... all the rest of the registers
    new_thread->state.r8 = 0;
    new_thread->state.r9 = 0;
    new_thread->state.r10 = 0;
    new_thread->state.r11 = 0;
    new_thread->state.r12 = 0;
    new_thread->state.r13 = 0;
    new_thread->state.r14 = 0;
    new_thread->state.r15 = 0;

    new_thread->stack = stack;
    new_thread->stacksize = stack_size;
    new_thread->tid = next_tid++;
    new_thread->status = MKTERMSTAT(LWP_LIVE, 0);
    new_thread->state.fxsave = FPU_INIT;
    new_thread->exited = NULL;
    new_thread->sched_one = NULL;

    /* uintptr_t end = (uintptr_t)new_thread->stack + new_thread->stacksize;
    uintptr_t top = end & ~0xFUL;             // 16-byte aligned

    uintptr_t A = top - 16;                   // A = location for saved_rbp
    *(uint64_t*)(A)     = 0;                  // saved RBP (fake)
    *(uint64_t*)(A + 8) = (uint64_t)(uintptr_t)lwp_wrap;  // RETURN ADDRESS

    new_thread->state.rbp = A;
    new_thread->state.rsp = A + 8; */


   /* Top of stack, 16B aligned */
uintptr_t top_aligned = ((uintptr_t)new_thread->stack + new_thread->stacksize) & ~0xFUL;

/* CRITICAL: rbp must be 8 mod 16 so that:
 *   leave; ret  -> entry %rsp == (rbp + 16) == 8 mod 16
 *   then lwp_wrap’s prologue push %rbp makes %rsp == 0 mod 16
 *   (required by SysV AMD64; avoids movaps crashes in libc/ncurses)
 */
uintptr_t rbp0 = top_aligned - 24;            /* 16n + 8 */

/* Fake frame: [rbp]=saved_rbp, [rbp+8]=retaddr */
*(uint64_t*)(rbp0)     = 0;                   /* saved RBP (fake) */
*(uint64_t*)(rbp0 + 8) = (uint64_t)(uintptr_t)lwp_wrap;  /* RET → lwp_wrap */

/* Load context */
new_thread->state.rbp = rbp0;
new_thread->state.rsp = rbp0;                 /* leave will set rsp=rbp, ret uses [rbp+8] */
new_thread->state.rdi = (uint64_t)(uintptr_t)function;  /* arg1 to lwp_wrap */
new_thread->state.rsi = (uint64_t)(uintptr_t)argument;  /* arg2 to lwp_wrap */

/* Optional sanity: ensure rbp0%16==8 and retaddr matches */
LWPDBG("CREATE: tid=%lu rbp=%p (mod16=%lu) ret=%p exp=%p",
       (unsigned long)new_thread->tid, (void*)rbp0, (unsigned long)(rbp0 & 0xF),
       (void*)(uintptr_t)*(uint64_t*)(rbp0+8), (void*)&lwp_wrap);
 

    dbg_dump_thread("create", new_thread);

    if (next_tid == 0)
        next_tid = 1;

    get_sched();
    // Add to global all threads

    new_thread->lib_one = NULL;
    new_thread->lib_two = NULL;

    all_add(&all_list_head, &all_list_tail, new_thread);
    if (curr_schedule && curr_schedule->admit)
    {
        int q = curr_schedule->qlen();
       // fprintf(stderr, "[CHK] after admit tid=%lu qlen=%d\n",
        //        (unsigned long)new_thread->tid, q);
        curr_schedule->admit(new_thread);
    }
    else
    {
        fifo_push(&running_head, &running_tail, new_thread);
    }

    return new_thread->tid;
} 



void lwp_start(void)
{
    if (system_started)
        return;

    get_sched();

    thread main_thread = (thread)malloc(sizeof(*main_thread));
    if (!main_thread)
    {
        perror("malloc");
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
    else
    {
        fifo_push(&running_head, &running_tail, main_thread);
    }

    system_started = 1;



    LWPDBG("BOOT: about to call swap_rfiles(save_from=?, load_to=main)");
    dbg_dump_thread("  main_thread(before)", main_thread);

/* If you intend to LOAD main_thread state here, it must already be valid.
   If you intend to SAVE ONLY, make sure you pass NULL in the load slot. */

    swap_rfiles(&main_thread->state, NULL);
    lwp_yield();
}

/**
 * @brief yields control to next thread
 */
void lwp_yield(void)
{

    thread next_thread = NULL;
    if (curr_schedule && curr_schedule->next)
    {
        next_thread = curr_schedule->next();
    }
    else 
    {
        next_thread = fifo_pop(&running_head, &running_tail);
    }
    // Terminate the program
    if (next_thread == NULL)
    {
        LWPDBG("yield: next=NULL (no more threads)");
        //fprintf(stderr, "NO MORE THREADS!!");
        exit(LWPTERMSTAT(curr_thread ? curr_thread->status : 0));
    }

    if (next_thread == curr_thread)
    {
        LWPDBG("yield: next==curr (no-op switch)");
        return;
    }

// --------- TESTINGG ----------- 
    LWPDBG("YIELD: scheduling switch curr=%p -> next=%p", (void*)curr_thread, (void*)next_thread);
    dbg_dump_thread("  curr(before)", curr_thread);
    dbg_dump_thread("  next(before)", next_thread);

    /* For user-created LWPs, confirm the fake frame for leave;ret is present */
    // dbg_check_retaddr("  next(retaddr)", next_thread, (void*)&lwp_wrap);

    /* Arg order audit */
    LWPDBG("YIELD: calling swap_rfiles(save_from=%p, load_to=%p)",
        (void*)&curr_thread->state, (void*)&next_thread->state);
// -------------------------------------------------
    LWPDBG("DEBUG: About to load thread %lu: rbp=%p rsp=%p rdi=%p rsi=%p",
    next_thread->tid, 
    (void*)next_thread->state.rbp,
    (void*)next_thread->state.rsp,
    (void*)next_thread->state.rdi,
    (void*)next_thread->state.rsi);

    thread prev = curr_thread;
    curr_thread = next_thread;

    if (prev)
    {
        swap_rfiles(&prev->state, &next_thread->state);
    }
    else
    {
        swap_rfiles(NULL, &next_thread->state);
    }
}

/**
 * 
 */
void lwp_exit(int exitval)
{
    int exit_status = exitval & 0xFF;
    curr_thread->status = MKTERMSTAT(LWP_TERM, exit_status);


    // [A] entering exit, before removing from scheduler
    LWPDBG("EXIT: enter tid=%lu status=0x%x", (unsigned long)curr_thread->tid, (unsigned)curr_thread->status);
    dbg_dump_thread("  exiting(before remove)", curr_thread);

    if (curr_schedule && curr_schedule->remove)
    {
        curr_schedule->remove(curr_thread);
    }

    // [B] after removing from scheduler
    LWPDBG("EXIT: removed from scheduler tid=%lu", (unsigned long)curr_thread->tid);
    dbg_dump_thread("  exiting(after remove)", curr_thread);

    // If waiting, give this thread to the waiter
    thread waiter = fifo_pop(&waiting_head, &waiting_tail);
    if (waiter)
    {
                // [C] handing off to a waiter
        LWPDBG("EXIT: waking waiter=%p tid=%lu with exited=%p tid=%lu",
               (void*)waiter, (unsigned long)waiter->tid,
               (void*)curr_thread, (unsigned long)curr_thread->tid);
        dbg_dump_thread("  waiter(before admit)", waiter);


        waiter->exited = curr_thread;
        if (curr_schedule && curr_schedule->admit)
        {
            curr_schedule->admit(waiter);

        }
        else
        {
            fifo_push(&running_head, &running_tail, waiter);
        }
        // [D] after rescheduling waiter
        LWPDBG("EXIT: rescheduled waiter=%p tid=%lu", (void*)waiter, (unsigned long)waiter->tid);
    }
    else 
    {
        // [E] no waiter, enqueue to terminated FIFO
        LWPDBG("EXIT: enqueue terminated t=%p tid=%lu to term FIFO",
               (void*)curr_thread, (unsigned long)curr_thread->tid);
        // queue this as a terminated thread
        fifo_push(&term_head, &term_tail, curr_thread);
    }
    // [F] final yield — this never returns
    LWPDBG("EXIT: yielding away from tid=%lu", (unsigned long)curr_thread->tid);
    lwp_yield(); // never returns
}

tid_t lwp_wait(int *status)
{
    // [1] entry
    LWPDBG("WAIT: enter caller tid=%lu", (unsigned long)(curr_thread ? curr_thread->tid : 0));
    dbg_dump_thread("  caller", curr_thread);
    // If any terminated threads exist, return OLDEST
    thread dead = fifo_pop(&term_head, &term_tail);

    if (dead)
    {
        LWPDBG("WAIT: returning oldest dead=%p tid=%lu", (void*)dead, (unsigned long)dead->tid);
        dbg_dump_thread("  dead(pop)", dead);

        tid_t id = dead->tid;
        if (status)
            *status = LWPTERMSTAT(dead->status);
        
        // Unlink from global list
        all_remove(&all_list_head, &all_list_tail, dead);
        // free resources -- don't unmap original
        if (dead->stack && dead->stacksize)
        {
            LWPDBG("WAIT: munmap dead stack tid=%lu addr=%p size=%lu",
                (unsigned long)dead->tid, dead->stack, (unsigned long)dead->stacksize);
            munmap(dead->stack, dead->stacksize);
        }
        free(dead);

        LWPDBG("WAIT: return tid=%lu (non-blocking)", (unsigned long)id);

        return id;
    }


    // Block: remove from scheduler, enqueue on waiting fifo, yield
    if (curr_schedule && curr_schedule->remove)
        curr_schedule->remove(curr_thread);

    curr_thread->exited = NULL;

    LWPDBG("WAIT: blocking caller tid=%lu -> waiting FIFO", (unsigned long)curr_thread->tid);

    fifo_push(&waiting_head, &waiting_tail, curr_thread);

    LWPDBG("WAIT: resumed tid=%lu exited=%p", (unsigned long)curr_thread->tid, (void*)curr_thread->exited);

    lwp_yield();


    // Consume the specific dead thread handed to us
    LWPDBG("WAIT: resumed tid=%lu exited=%p", (unsigned long)curr_thread->tid, (void*)curr_thread->exited);

    dead = curr_thread->exited;
    if (dead)
    {
        dbg_dump_thread("  dead(resume)", dead);

        tid_t id = dead->tid;
        if (status)
            *status = LWPTERMSTAT(dead->status);

        all_remove(&all_list_head, &all_list_tail, dead);
        if (dead->stack && dead->stacksize)
        {
            LWPDBG("WAIT: munmap dead stack tid=%lu addr=%p size=%lu",
                   (unsigned long)dead->tid, dead->stack, (unsigned long)dead->stacksize);
            munmap(dead->stack, dead->stacksize);
        }
        free(dead);
        curr_thread->exited = NULL;

        LWPDBG("WAIT: return tid=%lu (after block)", (unsigned long)id);
        return id;
    }
    LWPDBG("WAIT: resumed but no exited thread set! returning NO_THREAD");
    return NO_THREAD;
}

tid_t lwp_gettid(void)
{
    return curr_thread ? curr_thread->tid : NO_THREAD;
}

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
 * - also inits() the scheduler if needed
 * - 
 */ 
void lwp_set_scheduler(scheduler new_scheduler)
{
    // If NULL or the new schedule == default scheuler
    if (new_scheduler == curr_schedule)
    {
        //fprintf(stderr, "The new scheduler is already the current!");
        return;
    }

    if (new_scheduler == NULL)
    {
        curr_schedule = default_scheule;
    }
    else 
    {
        curr_schedule = new_scheduler;
    }
}

scheduler lwp_get_scheduler(void)
{
    return curr_schedule;
}
