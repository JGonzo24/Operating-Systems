# Makefile

# Variables
APP       := app
SRC_DIR   := src
INC_DIR   := include
BUILD_DIR := build

# Compiler Setting
CC   := gcc
CPPFLAGS := -I$(INC_DIR)        # preprocessing, look inside include/ for headers
CFLAGS   := -Wall -Wextra -O2 -g   # Wall: enable a broad set of common warnings, -Wextra: even more warnings not covered by -wall, -02: good level of optimization, 
LDFLAGS  := 
LDLIBS   := 

CFLAGS += -MMD -MP
DEP := $(OBJ:.o=.d)
-include $(DEP)


# Auto discover sources
SRC := $(wildcard $(SRC_DIR)/*.c)  # Wildcards: It will expand file patterns into a space-separated list of matching files


# patsubst ("pattern substitute")
# Pattern syntax uses '%' as a stem (the variable part that matches)
# $(patsubst FROM, TO, LIST), replaces each item matching FROM with TO


OBJ := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC))
.PHONY: show-obj
show-obj:
	@echo "Object files I will build:"
	@printf " %s\n" $(OBJ) 



#1) Define how to create the directory
$(BUILD_DIR):
	mkdir -p $@

# 2) Use it as an *order-only* prerequisite for compile rules (and link rules).
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@


# BIN relies on the others
BIN := $(BUILD_DIR)/$(APP)

# default target should *build* your program, not echo
.PHONY: all
all: $(BIN)

# link: all objects -> final binary
$(BIN): $(OBJ) | $(BUILD_DIR)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)
