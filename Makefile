CC := gcc

CFLAGS := -std=c11 -Wall -Wextra -g

TARGET_EXEC := compiler

# Automagic file discovery
SRCS := $(wildcard src/*.c *.c)
OBJS := $(patsubst %.c,%.o,$(SRCS))

.PHONY: build clean

build: $(TARGET_EXEC)

clean:
	rm -f *.o src/*.o $(TARGET_EXEC)

# Compilation rules
$(TARGET_EXEC): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^           

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@