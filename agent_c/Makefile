CC ?= gcc
CFLAGS := -std=c11 -Wall -Wextra -Werror -O2 -Iinclude

SRCS := src/main.c src/proc_net_dev.c src/uptime.c src/ping.c src/json_writer.c
OBJS := $(SRCS:.c=.o)

.PHONY: all build clean test_c run fmt lint forbid_system

all: build
build: netdiag_agent
netdiag_agent: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
test_c: build
	$(CC) $(CFLAGS) tests/test_proc_net_dev.c src/proc_net_dev.c -o test_proc_net_dev
	./test_proc_net_dev
run: build
	./netdiag_agent
fmt:
	clang-format -i $(SRCS) include/*.h tests/*.c
lint:
	@tmp=$$(mktemp -d); cp -r src include tests "$$tmp"/; \
	clang-format -i "$$tmp"/src/*.c "$$tmp"/include/*.h "$$tmp"/tests/*.c; \
	diff -ru src "$$tmp"/src >/dev/null || (echo "C format mismatch in src"; exit 1); \
	diff -ru include "$$tmp"/include >/dev/null || (echo "C format mismatch in include"; exit 1); \
	diff -ru tests "$$tmp"/tests >/dev/null || (echo "C format mismatch in tests"; exit 1); \
	rm -rf "$$tmp"
forbid_system:
	@if grep -R --line-number "system(" src include >/dev/null 2>&1; then echo "ERROR: system() usage found"; exit 1; fi
clean:
	rm -f netdiag_agent test_proc_net_dev $(OBJS)
