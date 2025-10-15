# Tool and flags
CC := gcc
CFLAGS := -Wall -Wextra  -fPIC -I.

# ===== Library source and object files =====
LIB_SRCS := liblwp.c schedulers.c magic64.S
LIB_OBJS := liblwp.o schedulers.o magic64.o
LIB_HDRS := lwp.h schedulers.h

.PHONY: all clean

# Default target
all: liblwp.so

# Build shared library
liblwp.so: $(LIB_OBJS)
	$(CC) -shared -o $@ $^

# Compile C source files (depend on headers)
%.o: %.c $(LIB_HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile assembly source files
%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up 
clean:
	rm -f $(LIB_OBJS) liblwp.so