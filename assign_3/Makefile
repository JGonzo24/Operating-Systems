CC     := gcc
CFLAGS := -Wall -g -DNUM_PHILOSOPHERS
LDFLAGS := -lpthread -lrt

target := dine

NUM_PHILOSOPHERS = 5

$(target): dine.o dawdle.o 
	$(CC) $(LDFLAGS) $^ -o $@

dine.o: dine.c dine.h
	$(CC) $(CFLAGS)=$(NUM_PHILOSOPHERS) -c $< -o $@

dawdle.o: dawdle.c
	$(CC) $(CFLAGS) -c $^ -o $@

trivial: trivial.o
	$(CC) $(LDFLAGS) $^ -o $@

trivial.o: trivial_pt.c
	$(CC) $(CFLAGS) -c $^ -o $@

.PHONY: clean, dine
clean:
	rm -f *.o


