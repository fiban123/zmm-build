CC      := clang
CFLAGS  := -std=gnu23 -Wall -Wextra -fPIC -Iinclude -Iextern
LDFLAGS := 
SO_LIBS := -lpthread -lcpu_features
EXE_LIBS := -lzmm -lcpu_features

# --- Performance Mode Logic ---
ifeq ($(filter fast, $(MAKECMDGOALS)),)
    # Default (Debug/Sanitized)
else
    CFLAGS += -O3
endif

# --- Install Prefix ---
PREFIX  ?= /usr/local
BIN_DIR := $(PREFIX)/bin
LIB_DIR := $(PREFIX)/lib
INC_DIR := $(PREFIX)/include/zmm

# --- Directories ---
SRC_DIR := src
CLI_DIR := cli
BLD_DIR := build
OBJ_DIR := $(BLD_DIR)/.obj
DEP_DIR := $(BLD_DIR)/.deps

# --- OS Specifics ---
ifeq ($(OS),Windows_NT)
    SHLIB_EXT   := .dll
    EXE_EXT     := .exe
else
    SHLIB_EXT   := .so
    EXE_EXT     := 
endif

# --- Files ---
# Library sources
SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
DEPS := $(SRCS:$(SRC_DIR)/%.c=$(DEP_DIR)/%.d)

# CLI sources
CLI_SRCS := $(wildcard $(CLI_DIR)/*.c)
CLI_OBJS := $(CLI_SRCS:$(CLI_DIR)/%.c=$(OBJ_DIR)/cli_%.o)
CLI_DEPS := $(CLI_SRCS:$(CLI_DIR)/%.c=$(DEP_DIR)/cli_%.d)

LIB        := $(BLD_DIR)/libzmm$(SHLIB_EXT)
TEST       := $(BLD_DIR)/test.out
CLI_EXE    := $(BLD_DIR)/cli$(EXE_EXT)  

DEPFLAGS = -MMD -MP -MF $(DEP_DIR)/$(notdir $*).d

.PHONY: all lib test cli clean fast lto-fast install uninstall

all: lib test cli

fast: all
lto-fast: all

# Shared Library Target
lib: $(LIB)

$(LIB): $(OBJS)
	@mkdir -p $(BLD_DIR)
	$(CC) $(LDFLAGS) -shared -o $@ $^ $(SO_LIBS)

# CLI Target
cli: $(CLI_EXE)

$(CLI_EXE): $(CLI_OBJS) $(LIB)
	$(CC) $(CLI_OBJS) -o $@ -L$(BLD_DIR) $(LDFLAGS) $(EXE_LIBS)

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
	$(CC) $(CFLAGS) $< -o $@ -L$(BLD_DIR) $(LDFLAGS) $(EXE_LIBS)

clean:
	rm -rf $(OBJ_DIR)
	rm -f $(LIB)
	rm -f $(CLI_EXE)
	rm -f $(TEST)

# --- Installation Targets ---
install: fast
	@echo "Installing zmm to $(PREFIX)..."
	@install -d $(BIN_DIR) $(LIB_DIR) $(INC_DIR)
	
	@echo " -> Installing CLI as zmm$(EXE_EXT)"
	@install -m 755 $(CLI_EXE) $(BIN_DIR)/zmm$(EXE_EXT)

ifeq ($(OS),Windows_NT)
	@echo " -> Installing Shared Library to $(BIN_DIR) (Windows PATH requirement)"
	@install -m 755 $(LIB) $(BIN_DIR)/
else
	@echo " -> Installing Shared Library to $(LIB_DIR)"
	@install -m 644 $(LIB) $(LIB_DIR)/
endif
	
	@echo " -> Installing Headers to $(INC_DIR)"
	@install -m 644 include/*.h $(INC_DIR)/
	
	@echo "Installation complete!"

uninstall:
	@echo "Uninstalling zmm..."
	rm -f $(BIN_DIR)/zmm$(EXE_EXT)
	rm -f $(BIN_DIR)/libzmm$(SHLIB_EXT)
	rm -f $(LIB_DIR)/libzmm$(SHLIB_EXT)
	rm -rf $(INC_DIR)
	@echo "Uninstallation complete!"

-include $(DEPS)
-include $(CLI_DEPS)
