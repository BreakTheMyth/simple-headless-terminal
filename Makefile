.PHONY: all clean

.DEFAULT_GOAL := all

CC 	   = gcc
CFLAGS = -Wall -MMD -MP -O2 $(shell find ./lib -name "include" -type d | awk '{print "-I"$$1}') -I./src
TARGET = sht
LIBS   = $(shell find ./lib -name "*.c" | grep src)
SRCS   = $(shell find ./src -name "*.c")
OBJS   = $(SRCS:%.c=%.o) $(LIBS:%.c=%.o)
DEPS   = $(OBJS:.o=.d)

-include $(DEPS)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean: 
	rm -f $(TARGET) $(OBJS) $(DEPS)