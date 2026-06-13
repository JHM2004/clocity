# cloc-c Makefile
# Build: make          (uses GCC/Clang)
# Clean: make clean

CC      = gcc
CFLAGS  = -std=c11 -O2 -Wall -Wextra -Iinclude
LDFLAGS =

SRCDIR  = src
OBJDIR  = obj
TARGET  = clocc

# Source files (exclude platform-specific files for current OS)
SRC_COMMON = $(SRCDIR)/main.c \
             $(SRCDIR)/scanner.c \
             $(SRCDIR)/counter.c \
             $(SRCDIR)/language.c \
             $(SRCDIR)/output.c \
             $(SRCDIR)/thread.c \
             $(SRCDIR)/utils.c

ifeq ($(OS),Windows_NT)
    SRC_OS = $(SRCDIR)/os_win32.c
    LDFLAGS += -lkernel32
else
    SRC_OS = $(SRCDIR)/os_unix.c
    LDFLAGS += -lpthread
endif

SOURCES = $(SRC_COMMON) $(SRC_OS)
OBJECTS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SOURCES))

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR) $(TARGET)

test: $(TARGET)
	./$(TARGET) src/

# Debug build
debug: CFLAGS += -g -DDEBUG -O0
debug: clean $(TARGET)

# Release build with optimizations
release: CFLAGS += -O3 -DNDEBUG -flto
release: LDFLAGS += -flto
release: clean $(TARGET)
