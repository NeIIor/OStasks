CC ?= gcc

TARGET := task10
SRCS := main.c
OBJS := $(SRCS:.c=.o)

BASE_CFLAGS := -std=c11 -O2 -Wall -Wextra -Wpedantic

LDFLAGS :=
LDLIBS :=

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(BASE_CFLAGS) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJS)

test: $(TARGET)
	@echo "A AND B" | ./$(TARGET)

