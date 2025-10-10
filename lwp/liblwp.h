#ifndef LIB_LWP_H
#define LIB_LWP_H

#include "lwp.h" 
#include <sys/types.h>


typedef struct{
    thread next;
    thread head;
    size_t size;
} thread_pool;

tid_t lwp_create(lwpfun function, void *argument);
void lwp_start(void);
void lwp_yield(void);
void lwp_exit(int exitval);
tid_t lwp_yield(int exitval);
tid_t lwp_wait(int status);
tid_t lwp_gettid(void);
thread tid2thread(void);
void lwp_set_scheduler(scheduler sched);
scheduler lwp_get_scheduler(void);

#endif // LIB_LWP_H