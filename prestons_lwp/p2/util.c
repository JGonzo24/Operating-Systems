#define _GNU_SOURCE
#include "lwp.h"
#include <unistd.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <stddef.h>

static size_t round_up(size_t x, size_t a){ return (x + a-1) & ~(a-1); }

static size_t default_stack_size(void){
  struct rlimit rl;
  long pg = sysconf(_SC_PAGESIZE);
  size_t sz = (getrlimit(RLIMIT_STACK,&rl)==0 && rl.rlim_cur>0)
					 ? rl.rlim_cur : (8ul<<20);
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

