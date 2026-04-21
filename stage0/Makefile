CC      ?= cc
CFLAGS  ?= -std=c99 -Wall -Wextra -Wpedantic -g -O0
SRCS    := main.c lexer.c
OBJS    := $(SRCS:.c=.o)
TARGET  := kaic0

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

test: $(TARGET)
	@echo "no tests yet (milestone 1 skeleton)"
