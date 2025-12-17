.DEFAULT_GOAL := all
CC := cc
CFLAGS := -Wall -W -O2 -Iinclude
LDFLAGS := -lm

SRC_DIR := src
OBJ_DIR := obj
INCLUDE_DIR := include
BUILD_DIR := build
TEST_DIR := tests

SRCS = $(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_DIR)/evaluator/*.c) $(wildcard $(SRC_DIR)/parser/*.c)
# Add external sources for Windows
ifeq ($(OS),Windows_NT)
    SRCS += $(wildcard $(SRC_DIR)/external/*.c)
endif
# convert all source paths to object paths
OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))
EXEC = $(BUILD_DIR)/cq

# Library objects (everything except main.c)
LIB_SRCS = $(filter-out $(SRC_DIR)/main.c, $(SRCS))
LIB_OBJS = $(LIB_SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# Test files
TEST_SRCS = $(wildcard $(TEST_DIR)/*.c)
TEST_EXECS = $(TEST_SRCS:$(TEST_DIR)/%.c=$(BUILD_DIR)/%)

# Detect OS
ifeq ($(OS),Windows_NT)
    MKDIR = if not exist
    RM = rmdir /s /q
    SEP = \\
else
    MKDIR = mkdir -p
    RM = rm -rf
    SEP = /
endif

all: $(EXEC)

$(EXEC): $(OBJS) | $(BUILD_DIR)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Rule for parser sources
$(OBJ_DIR)/parser/%.o: $(SRC_DIR)/parser/%.c | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Rule for evaluator sources
$(OBJ_DIR)/evaluator/%.o: $(SRC_DIR)/evaluator/%.c | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Rule for external sources (Windows mmap implementation)
$(OBJ_DIR)/external/%.o: $(SRC_DIR)/external/%.c | $(OBJ_DIR)
ifeq ($(OS),Windows_NT)
	@if not exist $(OBJ_DIR)\\external mkdir $(OBJ_DIR)\\external
endif
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
ifeq ($(OS),Windows_NT)
	@if not exist $(OBJ_DIR) mkdir $(OBJ_DIR)
	@if not exist $(OBJ_DIR)\\parser mkdir $(OBJ_DIR)\\parser
	@if not exist $(OBJ_DIR)\\evaluator mkdir $(OBJ_DIR)\\evaluator
else
	@mkdir -p $(OBJ_DIR)
	@mkdir -p $(OBJ_DIR)/parser
	@mkdir -p $(OBJ_DIR)/evaluator
endif

$(BUILD_DIR):
ifeq ($(OS),Windows_NT)
	@if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
else
	@mkdir -p $(BUILD_DIR)
endif

# Build test executables
$(BUILD_DIR)/test_%: $(TEST_DIR)/test_%.c $(LIB_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< $(LIB_OBJS) -o $@ $(LDFLAGS)

# Build all tests
tests: $(TEST_EXECS)

# Run all tests
test: tests
ifeq ($(OS),Windows_NT)
	@powershell -Command "$$tests = Get-ChildItem -Path build -Filter test_*.exe | Where-Object { $$_.Name -ne 'test_load_performance.exe' }; foreach ($$t in $$tests) { Write-Host ''; Write-Host \"=== Running $$($t.Name) ===\"  -ForegroundColor Cyan; & $$t.FullName; if ($$LASTEXITCODE -ne 0) { exit $$LASTEXITCODE } }"
else
	@for test in $(TEST_EXECS); do \
		if [ "$$(basename $$test)" = "test_load_performance" ] && [ ! -f data/bigdata.csv ]; then \
			echo "\n=== Generating bigdata.csv for performance test ==="; \
			python utils/generate_big_dataset.py 1000000; \
		fi; \
		echo "\n=== Running $$test ==="; \
		$$test || exit 1; \
	done
endif

clean:
ifeq ($(OS),Windows_NT)
	@if exist $(OBJ_DIR) rmdir /s /q $(OBJ_DIR)
	@if exist $(BUILD_DIR) rmdir /s /q $(BUILD_DIR)
else
	rm -rf $(OBJ_DIR) $(BUILD_DIR)
endif
	
run: $(EXEC)
	$(EXEC)

debug:
	$(CC) -g -Wall -W -Iinclude $(SRC_DIR)/*.c -o build/cq_debug

address_sanitizer:
	$(CC) -g -fsanitize=address -Wall -W -Iinclude $(SRC_DIR)/*.c -o build/cq_debug && ./build/cq_debug ./test_data.csv -p -q 'role "admin" EQ age 25 GT AND' 2>&1

bigdata:
	python utils/generate_big_dataset.py 1000000

.PHONY: all clean up env down build logs test repl bigdata