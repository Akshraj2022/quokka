CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wno-unused-parameter -O2
BUILD_DIR = build
TARGET = $(BUILD_DIR)/quokka

SRCS = src/bootstrap/main.c \
       src/bootstrap/lexer.c \
       src/bootstrap/parser.c \
       src/bootstrap/interpreter.c \
       src/bootstrap/sha256.c \
       src/joey/joey_native.c \
       src/joey/joey_watch.c \
       src/joey/joey_gui.c

UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)

ifeq ($(findstring MINGW,$(UNAME_S)),MINGW)
    LIBS = -lws2_32 -luser32 -lgdi32 -lcomdlg32
    TARGET := $(BUILD_DIR)/quokka.exe
else ifeq ($(findstring MSYS,$(UNAME_S)),MSYS)
    LIBS = -lws2_32 -luser32 -lgdi32 -lcomdlg32
    TARGET := $(BUILD_DIR)/quokka.exe
else ifeq ($(UNAME_S),Windows)
    LIBS = -lws2_32 -luser32 -lgdi32 -lcomdlg32
    TARGET := $(BUILD_DIR)/quokka.exe
else ifeq ($(UNAME_S),Darwin)
    LIBS = -lm -lpthread -ldl
else
    LIBS = -lm -lpthread -ldl
endif

.PHONY: all clean test

all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): $(SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $(SRCS) $(LIBS)

test: $(TARGET)
	@bash tests/run_tests.sh

clean:
	rm -f $(TARGET)
