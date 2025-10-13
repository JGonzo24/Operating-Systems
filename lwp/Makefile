# ===== Paths ================================================================
DEMO_DIR := Asgn2/demos
LIB_DIR  := Asgn2/lib64
INC_DIRS := . $(DEMO_DIR)

# ===== Tooling ==============================================================
CC       := gcc
WARN     := -Wall -Wextra
DBG      := -g -O0 -fPIC -fno-omit-frame-pointer
DEPFLAGS := -MMD -MP
CFLAGS   := $(WARN) $(DBG) $(DEPFLAGS) $(foreach d,$(INC_DIRS),-I$(d))
LDFLAGS  := -Wl,-rpath,'$$ORIGIN/../lib64'
LDLIBS   := -lncurses

# ===== Toggle prebuilt vs local LWP =========================================
USE_PREBUILT ?= 0

# Detect scheduler source (supports either filename)
SCHED_SRC := $(firstword $(wildcard rr_scheduler.c) $(wildcard schedulers.c))
ifeq ($(SCHED_SRC),)
  $(error No scheduler source found: expected rr_scheduler.c or schedulers.c)
endif

# Your local LWP sources (adjust as needed)
MY_LWP_SRCS := liblwp.c magic64.S $(SCHED_SRC)
MY_LWP_OBJS := $(patsubst %.c,%.o,$(patsubst %.S,%.o,$(MY_LWP_SRCS)))

# Choose linkage
ifeq ($(USE_PREBUILT),1)
  LWP_OBJS :=
  LWP_LIBS := -L$(LIB_DIR) -lPLN
else
  LWP_OBJS := $(MY_LWP_OBJS)
  LWP_LIBS :=
endif

# Snakes shared lib (for demos)
SNAKES_LIBS := -L$(LIB_DIR) -lsnakes

# ===== Demos ================================================================
HUNGRY_OBJS  := $(DEMO_DIR)/hungrysnakes.o $(DEMO_DIR)/util.o
RANDOM_OBJS  := $(DEMO_DIR)/randomsnakes.o $(DEMO_DIR)/util.o
NUMBERS_OBJS := $(DEMO_DIR)/numbersmain.o

.PHONY: all clean
all: hungry randomsnakes numbers

hungry: $(HUNGRY_OBJS) $(LWP_OBJS)
	$(CC) $(CFLAGS) -o $(DEMO_DIR)/$@ $^ $(SNAKES_LIBS) $(LWP_LIBS) $(LDLIBS) $(LDFLAGS)

randomsnakes: $(RANDOM_OBJS) $(LWP_OBJS)
	$(CC) $(CFLAGS) -o $(DEMO_DIR)/$@ $^ $(SNAKES_LIBS) $(LWP_LIBS) $(LDLIBS) $(LDFLAGS)

numbers: $(NUMBERS_OBJS) $(LWP_OBJS)
	$(CC) $(CFLAGS) -o $(DEMO_DIR)/$@ $^ $(SNAKES_LIBS) $(LWP_LIBS) $(LDLIBS) $(LDFLAGS)

# Optional standalone test
test: main.o $(LWP_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LWP_LIBS) $(LDLIBS) $(LDFLAGS)

# ===== Compile rules + auto-deps ============================================
$(DEMO_DIR)/%.o: $(DEMO_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

-include $(MY_LWP_OBJS:.o=.d) \
         $(HUNGRY_OBJS:.o=.d) \
         $(RANDOM_OBJS:.o=.d) \
         $(NUMBERS_OBJS:.o=.d) \
         main.d

# ===== Clean ================================================================
clean:
	rm -f $(DEMO_DIR)/hungry $(DEMO_DIR)/randomsnakes $(DEMO_DIR)/numbers test
	rm -f $(DEMO_DIR)/*.o $(DEMO_DIR)/*.d *.o *.d
