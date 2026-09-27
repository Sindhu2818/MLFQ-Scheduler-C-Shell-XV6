CC := gcc
CFLAGS := -std=c23 -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 \
          -Wall -Wextra -Werror -Wno-unused-parameter -fno-asm \
          -Iinclude
LDFLAGS :=

SRC_DIR := src
BUILD_DIR := build
TARGET := shell.out

SRCS = src/main.c src/prompt.c src/lexer.c src/hop.c src/reveal.c src/peek.c src/locate.c src/execute.c src/redirection.c src/pipeline.c src/sequential.c src/background.c src/activities.c src/terminal_control.c src/resume.c src/ping.c src/spy.c src/snoop.c
OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)