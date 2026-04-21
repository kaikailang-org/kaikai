CC      ?= cc
CFLAGS  ?= -std=c99 -Wall -Wno-unused-function -g -O0
STAGE0  := ../stage0/kaic0
TARGET  := kaic1
SRC     := compiler.kai

.PHONY: all clean test

all: $(TARGET)

$(STAGE0):
	$(MAKE) -C ../stage0 kaic0

build:
	@mkdir -p build

build/stage1.c: $(SRC) $(STAGE0) | build
	$(STAGE0) $(SRC) > $@

$(TARGET): build/stage1.c
	$(CC) $(CFLAGS) -I ../stage0 $< -o $@

# End-to-end: stage 1 must at least reach the placeholder body on every
# example used to verify stage 0. Milestones 6+ replace this with real
# compilation, byte-compared against stage 0 output.
test: $(TARGET) build
	@set -e; \
	for f in ../examples/minimal/*.kai; do \
	  echo "=== stage1 on $$(basename $$f) ==="; \
	  ./$(TARGET) $$f > /dev/null; \
	done
	@echo "stage1 skeleton ran to completion on all examples."

clean:
	rm -rf build $(TARGET)
