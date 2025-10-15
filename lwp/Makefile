# ===== Paths =====
DEMO_DIR := Asgn2/demos
SNAKES_LIBDIR := Asgn2/lib64
SNAKES_INCDIR := Asgn2/include
OBJDIR := build
BINDIR := bin

# Tool and flags
CC := gcc
CFLAGS := -Wall -Wextra -O2 -fPIC -I. -I$(SNAKES_INCDIR)
NCURSES_LIB ?= ncursesw
LDLIBS := -L. -llwp -L$(SNAKES_LIBDIR) -lsnakes -l$(NCURSES_LIB)

# ===== Your library =====
LIB_SRCS := liblwp.c schedulers.c magic64.S
LIB_OBJS := $(patsubst %.c,$(OBJDIR)/%.o,$(patsubst %.S,$(OBJDIR)/%.o,$(LIB_SRCS)))

# ===== Demos (compile Given .c to our build dir) =====
HUNGRY_OBJS  := $(OBJDIR)/hungrysnakes.o $(OBJDIR)/util.o
RANDS_OBJS   := $(OBJDIR)/randomsnakes.o $(OBJDIR)/util.o
NUMBERS_OBJS := $(OBJDIR)/numbersmain.o

.PHONY: all clean run-hungry run-randomsnakes run-numbers
all: $(BINDIR)/hungry $(BINDIR)/randomsnakes $(BINDIR)/numbers

# --- library ---
liblwp.so: $(LIB_OBJS)
	$(CC) -shared -o $@ $^

# --- demos (link against your liblwp.so) ---
$(BINDIR)/hungry: liblwp.so $(HUNGRY_OBJS) | $(BINDIR)
	$(CC) -o $@ $(HUNGRY_OBJS) $(LDLIBS)

$(BINDIR)/randomsnakes: liblwp.so $(RANDS_OBJS) | $(BINDIR)
	$(CC) -o $@ $(RANDS_OBJS) $(LDLIBS)

$(BINDIR)/numbers: liblwp.so $(NUMBERS_OBJS) | $(BINDIR)
	$(CC) -o $@ $(NUMBERS_OBJS) $(LDLIBS)

# --- compile rules ---
$(OBJDIR) $(BINDIR):
	mkdir -p $@

$(OBJDIR)/%.o: %.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: %.S | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: $(DEMO_DIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -I$(DEMO_DIR) -c $< -o $@

# --- run targets: set LD_LIBRARY_PATH inline ---
define RUN_WITH_LIBS
	LD_LIBRARY_PATH="$(PWD):$(PWD)/$(SNAKES_LIBDIR):$$LD_LIBRARY_PATH" $1
endef

run-hungry: $(BINDIR)/hungry
	$(call RUN_WITH_LIBS,./$(BINDIR)/hungry)

run-randomsnakes: $(BINDIR)/randomsnakes
	$(call RUN_WITH_LIBS,./$(BINDIR)/randomsnakes)

run-numbers: $(BINDIR)/numbers
	$(call RUN_WITH_LIBS,./$(BINDIR)/numbers)

# --- clean ---
clean:
	rm -rf $(OBJDIR) $(BINDIR) liblwp.so
