CC=gcc
CFLAGS=-Wall -Wextra -O2 -g -std=gnu11 -fno-omit-frame-pointer -mno-omit-leaf-frame-pointer -pthread -fPIC
INCLUDE=-I.

# objects for the static lib (now includes smartalloc)
LIBOBJ=lwp.o sched_rr.o magic64.o smartalloc.o

LIB=liblwp.so
#was lwp.a
# libraries to link test programs with
LIBS=-L. -llwp -lpthread

# Use '>' instead of a TAB to start recipe lines
.RECIPEPREFIX := >

.PHONY: all clean tests

all: $(LIB)

# > @ar rcs $@ $^
$(LIB): $(LIBOBJ)
> $(CC) -shared -o $@ $^
	
# generic C -> .o rule (most sources depend on lwp.h and schedulers.h)
%.o: %.c lwp.h sched_rr.h
> $(CC) $(CFLAGS) $(INCLUDE) -c -o $@ $<

# smartalloc has its own header dependency
smartalloc.o: smartalloc.c smartalloc.h
> $(CC) $(CFLAGS) $(INCLUDE) -c -o $@ $<

# lwp.c specifically needs schedulers.h
lwp.o: lwp.c lwp.h sched_rr.h
> $(CC) $(CFLAGS) $(INCLUDE) -c -o $@ $<

# sched_rr.c needs both headers
sched_rr.o: sched_rr.c lwp.h sched_rr.h
> $(CC) $(CFLAGS) $(INCLUDE) -c -o $@ $<

# assemble the context switch
magic64.o: magic64.S
> $(CC) -c -o $@ $<

tests: $(LIB)
> $(CC) $(CFLAGS) $(INCLUDE) tests/t_create_yield.c $(LIBS) -o t_create_yield
> $(CC) $(CFLAGS) $(INCLUDE) tests/t_exit_wait.c   $(LIBS) -o t_exit_wait
> $(CC) $(CFLAGS) $(INCLUDE) tests/t_schedswap.c  $(LIBS) -o t_schedswap
> [ -f tests/t_wait.c ] && $(CC) $(CFLAGS) $(INCLUDE) tests/t_wait.c $(LIBS) -o t_wait || true

clean:
> rm -f $(LIBOBJ) $(LIB) t_create_yield t_exit_wait t_schedswap t_wait

snakes: $(LIB)
> $(CC) $(CFLAGS) $(INCLUDE) randomsnakes.c -L. -llwp libsnakes.a -lncurses -lpthread -o randomsnakes

