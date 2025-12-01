
CC = gcc
CFLAGS = -Wall -Wextra -g -std=c99

all: minget minls 

minget: minget.o utils.o
	$(CC) $(CFLAGS) utils.o minget.o -o minget

minls: minls.o utils.o
	$(CC) $(CFLAGS) utils.o minls.o -o minls

minget.o: minget.c minget.h
	$(CC) $(CFLAGS) -c minget.c 

minls.o: minls.c minls.h
	$(CC) $(CFLAGS) -c minls.c 

utils.o: utils.c
	$(CC) $(CFLAGS) -c utils.c

clean:
	rm -f *.o minget minls

.PHONY: all clean


