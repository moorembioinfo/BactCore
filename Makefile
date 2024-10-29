# Makefile

CC = gcc
CFLAGS = -O3 -march=native -flto -funroll-loops -ffast-math
LDFLAGS =


SRCDIR = ./src
BINDIR = ./

# Source files
SRCS = $(SRCDIR)/BactCore.c

# Target executable
TARGET = BactCore

# Build rule
$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET) $(LDFLAGS)

# Clean rule
clean:
	rm -f $(TARGET)
