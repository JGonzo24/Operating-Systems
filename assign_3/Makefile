CC     := gcc
CFLAGS := -Wall -O0 -Wextra -std=gnu99 -g -lpthread
LDFLAGS := -lpthread -lrt

target := dine

NUM_PHILOSOPHERS = 5

$(target): dine.o dawdle.o 
	$(CC) $(LDFLAGS) $^ -o $@

dine.o: dine.c dine.h
	$(CC) $(CFLAGS) -DNUM_PHILOSOPHERS=$(NUM_PHILOSOPHERS) -c $< -o $@

dawdle.o: dawdle.c dawdle.h
	$(CC) $(CFLAGS) -c $< -o $@

trivial: trivial.o
	$(CC) $(LDFLAGS) $^ -o $@

trivial.o: trivial_pt.c
	$(CC) $(CFLAGS) -c $^ -o $@

.PHONY: clean, dine
clean:
	rm -f *.o
	rm dine


