CC       := gcc
AR       := ar
CFLAGS   := -Wall -g -fPIC
BUILD    := build

.PHONY: all libs intel-all clean
all: libs bin/main

# Build all libs (static + shared)
libs: lib/libmalloc.a lib64/libmalloc.a lib/libmalloc.so lib64/libmalloc.so
intel-all: lib/libmalloc.so lib64/libmalloc.so

# ----- OBJ ------
OBJ32  := $(BUILD)/malloc32.o
OBJ64  := $(BUILD)/malloc64.o
APP_OBJ:= $(BUILD)/main.o

# -- Executables --
bin:
	mkdir -p bin

# Prefer static archive for the app (and ensure bin/ exists first)
bin/main: | bin
bin/main: $(APP_OBJ) lib64/libmalloc.a
	$(CC) $(CFLAGS) -m64 -o $@ $(APP_OBJ) lib64/libmalloc.a

# If you want to link against the shared lib instead, use this recipe:
# bin/main: | bin
# bin/main: $(APP_OBJ) lib64/libmalloc.so
# 	$(CC) $(CFLAGS) -m64 -o $@ $(APP_OBJ) -Llib64 -lmalloc -Wl,-rpath,'$(PWD)/lib64'

# ----- LIB DIRS -----
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

# ----- SHARED LIBS -----
lib/libmalloc.so: lib $(OBJ32)
	$(CC) $(CFLAGS) -m32 -shared -Wl,--no-undefined -o $@ $(OBJ32)
lib64/libmalloc.so: lib64 $(OBJ64)
	$(CC) $(CFLAGS) -shared -Wl,--no-undefined -o $@ $(OBJ64)

# ----- OBJECTS -----
$(BUILD)/malloc32.o: malloc.c malloc.h | $(BUILD)
	$(CC) $(CFLAGS) -m32 -c -o $@ $<
$(BUILD)/malloc64.o: malloc.c malloc.h | $(BUILD)
	$(CC) $(CFLAGS) -m64 -c -o $@ $<
$(BUILD)/main.o: main.c malloc.h | $(BUILD)
	$(CC) $(CFLAGS) -m64 -c -o $@ $<

# ----- CLEAN -----
clean:
	rm -rf $(BUILD) lib lib64 bin

run: all
	export DEBUG_MALLOC=1
	LD_PRELOAD="$(PWD)/lib64/libmalloc.so" ./bin/main
