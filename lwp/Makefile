CC = gcc
CFLAGS = -Wall -Wextra -g -fPIC
LDFLAGS = 
LDLIBS  = 

TARGET = liblwp

all: $(TARGET)

$(TARGET): liblwp.so

# Add the .o files to the .so library
liblwp.so: liblwp.o 
$(CC) $(CFLAGS) -shared -o $@ $<

# Compile step here
liblwp.o: liblwp.c liblwp.h 
$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o liblwp.o

.PHONY: clean $(TARGET)

