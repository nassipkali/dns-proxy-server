# Compiler
CC = gcc
# Compiler flags
CFLAGS = -Wall -Wextra -g
# Libraries
LIBS = -levent

# Source files
SRCS = dns_proxy.c

# Object files
OBJS = $(SRCS:.c=.o)

# Executable name
TARGET = dns_proxy

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
