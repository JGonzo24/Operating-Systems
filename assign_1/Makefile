CC      := gcc
CFLAGS  := -Wall -Wextra -Iinclude
SRC     := main.c
OBJ     := $(SRC:.c=.o)

LDFLAGS :=
LDLIBS  :=

TARGET  := myprog

# Link Step
$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ (LDLIBS)

# Compile Step
%.o:%.c
	$(CC) $(CFLAGS) -c $< -o $@ 


clean:
	rm -f $(TARGET) $(OBJ)

run:$(TARGET)
	./$(TARGET)
	
