CC       := gcc
AR       := ar
CFLAGS   := -Wall -Wextra -g -fPIC \
            -fno-builtin -fno-builtin-malloc -fno-builtin-free \
            -fno-builtin-realloc -fno-builtin-calloc
BUILD    := build
INC_DIR  := /home/jgonz555/OS/assign_1/Asgn1/include 
CFLAGS   += -I$(INC_DIR)

LDFLAGS  += -L$(LIB_DIR) -Wl,-rpath,$(LIB_DIR)
LIB_DIR  := /home/jgonz555/OS/assign_1/Asgn1/lib64
LIB_DIR32 := /home/jgonz555/OS/assign_1/Asgn1/lib


# Only external libs here; DO NOT re-list libmalloc.a in LDLIBS (we pass it explicitly in the link rule)
LDLIBS   := -lpp

.PHONY: all libs clean run intel-all
all: libs bin/main

# Intel-specific build target
intel-all: lib/libmalloc.so lib64/libmalloc.so

# ----- OBJ ------
OBJ32   := $(BUILD)/malloc32.o
OBJ64   := $(BUILD)/malloc64.o
APP_OBJ := $(BUILD)/main.o

bin:
	mkdir -p bin
lib:
	mkdir -p lib
lib64:
	mkdir -p lib64
$(BUILD):
	mkdir -p $(BUILD)

# ----- STATIC LIBS -----
lib/libmalloc.a: lib $(OBJ32)
	$(AR) rcs $@ $(OBJ32)
lib64/libmalloc.a: lib64 $(OBJ64)
	$(AR) rcs $@ $(OBJ64)

# ----- SHARED LIBS (still build them, but we won’t use for run in this profile) -----
lib/libmalloc.so: lib $(OBJ32)
	$(CC) $(CFLAGS) -m32 -shared -o $@ $(OBJ32) -L$(LIB_DIR32) -Wl,-rpath,$(LIB_DIR32) -lpp
lib64/libmalloc.so: lib64 $(OBJ64)
	$(CC) $(CFLAGS) -shared -o $@ $(OBJ64) $(LDFLAGS) $(LDLIBS)

# ----- OBJECTS -----
$(BUILD)/malloc32.o: malloc.c malloc.h | $(BUILD)
	$(CC) $(CFLAGS) -m32 -c -o $@ $<
$(BUILD)/malloc64.o: malloc.c malloc.h | $(BUILD)
	$(CC) $(CFLAGS) -m64 -c -o $@ $<
$(BUILD)/main.o: main.c malloc.h | $(BUILD)
	$(CC) $(CFLAGS) -m64 -c -o $@ $<

# ----- LINK APP (link your malloc archive; libs AFTER objects) -----
bin/main: $(APP_OBJ) lib64/libmalloc.a | bin
	$(CC) -m64 -o $@ $(APP_OBJ) $(LDFLAGS) lib64/libmalloc.a $(LDLIBS)

clean:
	rm -rf $(BUILD) lib lib64 bin

# No LD_PRELOAD here; your malloc is linked in.
run: all
	DEBUG_MALLOC=1 ./bin/main
