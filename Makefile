.DEFAULT_GOAL := all
CC := cc
CFLAGS := -Wall -W -O2 -Iinclude
LDFLAGS := -lm

SRC_DIR := src
OBJ_DIR := obj
INCLUDE_DIR := include
BUILD_DIR := build
TEST_DIR := tests

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
EXEC = $(BUILD_DIR)/cq

# Library objects (everything except main.c)
LIB_SRCS = $(filter-out $(SRC_DIR)/main.c, $(SRCS))
LIB_OBJS = $(LIB_SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# Test files
TEST_SRCS = $(wildcard $(TEST_DIR)/*.c)
TEST_EXECS = $(TEST_SRCS:$(TEST_DIR)/%.c=$(BUILD_DIR)/%)

all: $(EXEC)

$(EXEC): $(OBJS) | $(BUILD_DIR)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@ $(LDFLAGS)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Build test executables
$(BUILD_DIR)/test_%: $(TEST_DIR)/test_%.c $(LIB_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< $(LIB_OBJS) -o $@ $(LDFLAGS)

# Build all tests
tests: $(TEST_EXECS)

# Run all tests
test: tests
	@for test in $(TEST_EXECS); do \
		if [ "$$(basename $$test)" = "test_load_performance" ] && [ ! -f data/bigdata.csv ]; then \
			echo "\n=== Generating bigdata.csv for performance test ==="; \
			python utils/generate_big_dataset.py 1000000; \
		fi; \
		echo "\n=== Running $$test ==="; \
		$$test || exit 1; \
	done

clean:
	rm -rf $(OBJ_DIR) $(BUILD_DIR)
	
run: $(EXEC)
	$(EXEC)

address_sanitizer:
	$(CC) -g -fsanitize=address -Wall -W -Iinclude $(SRC_DIR)/*.c -o build/cq_debug && ./build/cq_debug ./test_data.csv -p -q 'role "admin" EQ age 25 GT AND' 2>&1

bigdata:
	python utils/generate_big_dataset.py 1000000

.PHONY: all clean up env down build logs test repl bigdata