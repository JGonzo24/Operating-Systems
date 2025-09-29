# ---- toolchain ----
CC       := gcc
AR       := ar
ARFLAGS  := rcs
CFLAGS   := -Wall -Wextra -g
PIC_FLAG := -fPIC

# ---- headers ----
# Put malloc.h in project root (.) or switch this to 'include'
INC_DIR  := .
CFLAGS   += -I$(INC_DIR)

# ---- names & src ----
APPNAME  := myproj
APP_SRC  := main.c

LIBNAME  := malloc
LIB_SRC  := malloc.c

# ---- dirs ----
LIB32_DIR := lib
LIB64_DIR := lib64
BINDIR    := bin
B32       := build32
B64       := build64

# ---- objects ----
APP32_OBJ := $(APP_SRC:%.c=$(B32)/%.o)
APP64_OBJ := $(APP_SRC:%.c=$(B64)/%.o)
LIB32_OBJ := $(LIB_SRC:%.c=$(B32)/%.o)
LIB64_OBJ := $(LIB_SRC:%.c=$(B64)/%.o)

# ---- libs ----
LIB32_SO := $(LIB32_DIR)/lib$(LIBNAME).so
LIB64_SO := $(LIB64_DIR)/lib$(LIBNAME).so
LIB32_A  := $(LIB32_DIR)/lib$(LIBNAME).a
LIB64_A  := $(LIB64_DIR)/lib$(LIBNAME).a

# ---- app outputs ----
APP32_SHARED := $(BINDIR)/$(APPNAME)32_shared
APP64_SHARED := $(BINDIR)/$(APPNAME)64_shared
APP32_STATIC := $(BINDIR)/$(APPNAME)32_static
APP64_STATIC := $(BINDIR)/$(APPNAME)64_static

.PHONY: all libs apps clean dirs
all: libs apps

libs: $(LIB32_SO) $(LIB64_SO) $(LIB32_A) $(LIB64_A)
apps: $(APP32_SHARED) $(APP64_SHARED) $(APP32_STATIC) $(APP64_STATIC)

dirs:
	mkdir -p $(LIB32_DIR) $(LIB64_DIR) $(BINDIR) $(B32) $(B64)

# ---- compile rules ----
$(B32)/%.o: %.c | dirs
	$(CC) $(CFLAGS) -m32 -c -o $@ $<

$(B64)/%.o: %.c | dirs
	$(CC) $(CFLAGS) -m64 -c -o $@ $<

# library objects need PIC for .so (ok for .a too)
$(LIB32_OBJ) $(LIB64_OBJ): CFLAGS += $(PIC_FLAG)

# ---- link libs ----
$(LIB32_SO): $(LIB32_OBJ) | dirs
	$(CC) -m32 -shared -Wl,-soname,lib$(LIBNAME).so -o $@ $(LIB32_OBJ)

$(LIB64_SO): $(LIB64_OBJ) | dirs
	$(CC) -m64 -shared -Wl,-soname,lib$(LIBNAME).so -o $@ $(LIB64_OBJ)

$(LIB32_A): $(LIB32_OBJ) | dirs
	$(AR) $(ARFLAGS) $@ $(LIB32_OBJ)

$(LIB64_A): $(LIB64_OBJ) | dirs
	$(AR) $(ARFLAGS) $@ $(LIB64_OBJ)

# ---- link apps ----
# shared: add rpath so the binary finds ./lib or ./lib64 at runtime
$(APP32_SHARED): $(APP32_OBJ) $(LIB32_SO) | dirs
	$(CC) -m32 -o $@ $(APP32_OBJ) -L$(LIB32_DIR) -l$(LIBNAME) \
	      -Wl,-rpath,'$$ORIGIN/../$(LIB32_DIR)'

$(APP64_SHARED): $(APP64_OBJ) $(LIB64_SO) | dirs
	$(CC) -m64 -o $@ $(APP64_OBJ) -L$(LIB64_DIR) -l$(LIBNAME) \
	      -Wl,-rpath,'$$ORIGIN/../$(LIB64_DIR)'

# static: link directly to the .a
$(APP32_STATIC): $(APP32_OBJ) $(LIB32_A) | dirs
	$(CC) -m32 -o $@ $(APP32_OBJ) $(LIB32_A)

$(APP64_STATIC): $(APP64_OBJ) $(LIB64_A) | dirs
	$(CC) -m64 -o $@ $(APP64_OBJ) $(LIB64_A)

# ---- header deps (auto) ----
DEPS := $(APP32_OBJ:.o=.d) $(APP64_OBJ:.o=.d) $(LIB32_OBJ:.o=.d) $(LIB64_OBJ:.o=.d)
-include $(DEPS)

clean:
	rm -rf $(B32) $(B64) $(LIB32_DIR) $(LIB64_DIR) $(BINDIR)
