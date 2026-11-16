CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude
CORE = src/core/vitaplus.c
TEST = build/test_core

.PHONY: test clean

test: $(TEST)
	$(TEST)

$(TEST): $(CORE) tests/test_core.c include/vitaplus.h
	@mkdir -p build
	$(CC) $(CFLAGS) $(CORE) tests/test_core.c -o $(TEST)

clean:
	rm -rf build
