CC      := clang
AR      := ar
ARFLAGS := rcs
CFLAGS  := -std=c23 -Wall -Wextra -fPIC -Iinclude -Iextern -ggdb3 -fsanitize=address -fsanitize=undefined
LDFLAGS := -Lbuild -lzmm -lcpu_features -Wl,-rpath,'$$ORIGIN' -ggdb3 -fsanitize=address -fsanitize=undefined

# --- Performance Mode Logic ---
ifeq ($(filter fast lto-fast,$(MAKECMDGOALS)),)
    # Default (Debug/Sanitized)
else
    CFLAGS  := -std=c23 -Wall -Wextra -fPIC -Iinclude -Iextern -O3
    LDFLAGS := -Lbuild -lzmm -lcpu_features -Wl,-rpath,'$$ORIGIN' -O3
endif

ifeq ($(filter lto-fast,$(MAKECMDGOALS)),lto-fast)
    CFLAGS  += -flto
    LDFLAGS += -flto
    AR      := llvm-ar
endif

# --- Directories ---
SRC_DIR := src
CLI_DIR := cli
BLD_DIR := build
OBJ_DIR := $(BLD_DIR)/.obj
DEP_DIR := $(BLD_DIR)/.deps

# --- Files ---
# Library sources
SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
DEPS := $(SRCS:$(SRC_DIR)/%.c=$(DEP_DIR)/%.d)

# CLI sources
CLI_SRCS := $(wildcard $(CLI_DIR)/*.c)
CLI_OBJS := $(CLI_SRCS:$(CLI_DIR)/%.c=$(OBJ_DIR)/cli_%.o)
CLI_DEPS := $(CLI_SRCS:$(CLI_DIR)/%.c=$(DEP_DIR)/cli_%.d)

LIB        := $(BLD_DIR)/libzmm.so
STATIC_LIB := $(BLD_DIR)/libzmm.a
TEST       := $(BLD_DIR)/test.out
CLI_EXE    := $(BLD_DIR)/cli  

DEPFLAGS = -MMD -MP -MF $(DEP_DIR)/$(notdir $*).d

.PHONY: all lib slib test cli clean fast lto-fast

all: lib slib test cli

fast: all
lto-fast: all

# Shared Library Target
lib: $(LIB)

$(LIB): $(OBJS)
	@mkdir -p $(BLD_DIR)
	$(CC) $(LDFLAGS) -shared -o $@ $^

# Static Library Target
slib: $(STATIC_LIB)

$(STATIC_LIB): $(OBJS)
	@mkdir -p $(BLD_DIR)
	$(AR) $(ARFLAGS) $@ $^

# CLI Target
cli: $(CLI_EXE)

$(CLI_EXE): $(CLI_OBJS) $(LIB)
	$(CC) $(CLI_OBJS) -o $@ $(LDFLAGS)

# Library Object Rule
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR) $(DEP_DIR)
	$(CC) $(DEPFLAGS) $(CFLAGS) -DBUILDING_SO -c $< -o $@

# CLI Object Rule (Prefixed to avoid collisions)
$(OBJ_DIR)/cli_%.o: $(CLI_DIR)/%.c
	@mkdir -p $(OBJ_DIR) $(DEP_DIR)
	$(CC) -MMD -MP -MF $(DEP_DIR)/cli_$*.d $(CFLAGS) -c $< -o $@

test: $(TEST)

$(TEST): test/main.c $(LIB)
	@mkdir -p $(BLD_DIR)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -rf $(BLD_DIR)

-include $(DEPS)
-include $(CLI_DEPS)
