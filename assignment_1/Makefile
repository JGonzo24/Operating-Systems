CC      := gcc
CFLAGS  := -Wall -g -fPIC
TARGET  := malloc

.PHONY: all
all: $(TARGET) 
# intel-all creates both 64 and 32 bit versions of the 
# dynamic shared lib 
.PHONY: $(TARGET)
$(TARGET): libmalloc.a libmalloc.so
# ------- For intel-all ---------
intel-all: lib/libmalloc.so lib64/libmalloc.so
lib lib64: 
	mkdir -p $@

lib/libmalloc.so: malloc32.o | lib
	$(CC) $(CFLAGS) -m32 -shared -o $@ malloc32.o

lib64/libmalloc.so: malloc64.o | lib64
	$(CC) $(CFLAGS) -m64 -shared -o $@ malloc64.o

malloc32.o: malloc.c 
	$(CC) $(CFLAGS) -m32 -c $< -o $@

malloc64.o: malloc.c
	$(CC) $(CFLAGS) -m64 -c $< -o $@


# --------- For static library --------
libmalloc.a: malloc64.o 
	ar rcs $@ $<
# --------- For dynamic library -------
libmalloc.so: malloc64.o
	$(CC) $(CFLAGS) -shared -o $@ $<


# ------- For testing with main.c  ------
#main: malloc main.o
#	$(CC) -o $@ main.o -L. -lmalloc 
#main.o: main.c
#	$(CC) $(CFLAGS) -c $< -o $@ 


# --------- Other ---------
clean:
	rm -rf lib lib64
	rm -f *.o libmalloc.so libmalloc.a 

.PHONY: malloc clean intel-all