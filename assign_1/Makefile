CC      := gcc
CFLAGS  := -Wall -Wextra -fPIC -g -Iinclude
LDFLAGS :=
LDLIBS  := 

TARGET  := malloc
.PHONY: intel-all clean malloc
intel-all: lib/libmalloc.so lib64/libmalloc.so


$(TARGET): lib64/libmalloc.so lib64/libmalloc.a
	@echo "Done!"

# 64 bit version of static
lib64/libmalloc.a: malloc64.o | lib64
	ar rcs $@ $< 


# Ensure output directories exist
lib lib64:
	mkdir -p $@

# 32-bit shared lib
lib/libmalloc.so: malloc32.o | lib
	$(CC) -m32 -shared -o $@ $^ $(LDLIBS)

# 64-bit shared lib
lib64/libmalloc.so: malloc64.o | lib64
	$(CC) -shared -o $@ $^ $(LDLIBS)

# Objects
malloc32.o: malloc.c
	$(CC) $(CFLAGS) -m32 -c -o $@ $<

malloc64.o: malloc64.c
	$(CC) $(CFLAGS) -m64 -c -o $@ $<

clean:
	rm -f *.o lib/libmalloc.so lib64/libmalloc.so
