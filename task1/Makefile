CC ?= gcc

TARGET := task1
SRCS := main.c
OBJS := $(SRCS:.c=.o)

# Base warnings stay in-repo; user may extend via environment CFLAGS.
BASE_CFLAGS := -std=c11 -O2 -Wall -Wextra -Wpedantic

LDFLAGS :=
LDLIBS := -pthread

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(BASE_CFLAGS) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJS)

test: $(TARGET)
	@echo "3 2 1" | ./$(TARGET) 12

