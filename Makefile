CC := gcc
CFLAGS := -std=c99 -Wall -Wextra -Wno-unused-parameter -g
LDFLAGS := 
SRC_DIR := src
OBJ_DIR := build
BIN := mlox

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

.PHONY: all clean run test docs

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

run: $(BIN)
	./$(BIN)

clean:
	rm -rf $(OBJ_DIR) $(BIN) docs/html

TEST_SCRIPTS := $(wildcard tests/*.mlox)

test: $(BIN) $(TEST_SCRIPTS)
	@set -e; \
	for script in $(TEST_SCRIPTS); do \
	echo "Running $$script"; \
	./$(BIN) $$script > $$script.out && diff -u --strip-trailing-cr $${script}.expected $$script.out; \
	rm -f $$script.out; \
	done

docs:
	doxygen Doxyfile
