# Makefile for UDP Receiver

# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -Wextra -O2

# Target executable
TARGET = udp_rx udp_tx

# Source files
SRCS = udp_rx.c udp_tx.c

# Object files
OBJS = $(SRCS:.c=.o)

all:
	gcc -Wall -Wextra -O2 udp_tx.c -o udp_tx
	gcc -Wall -Wextra -O2 udp_rx.c -o udp_rx

# Clean build artifacts
clean:
	rm -f $(OBJS) $(TARGET)

# Run the program
run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run