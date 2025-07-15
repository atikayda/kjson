# Makefile for kJSON C library

CC = gcc
AR = ar
CFLAGS = -Wall -Wextra -O2 -fPIC -std=c11
LDFLAGS = -shared

# Source files
SOURCES = kjson.c kjson_stringify.c kjson_binary.c
OBJECTS = $(SOURCES:.c=.o)

# Library names
STATIC_LIB = libkjson.a
SHARED_LIB = libkjson.so

# Targets
all: $(STATIC_LIB) $(SHARED_LIB)

# Static library
$(STATIC_LIB): $(OBJECTS)
	$(AR) rcs $@ $^

# Shared library
$(SHARED_LIB): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^

# Object files
%.o: %.c kjson.h
	$(CC) $(CFLAGS) -c $< -o $@

# Test program
test: test_kjson
	./test_kjson

test_kjson: test_kjson.c $(STATIC_LIB)
	$(CC) $(CFLAGS) -o $@ $< -L. -lkjson -lm

# Clean
clean:
	rm -f $(OBJECTS) $(STATIC_LIB) $(SHARED_LIB) test_kjson

# Install
PREFIX ?= /usr/local
install: $(STATIC_LIB) $(SHARED_LIB)
	install -d $(PREFIX)/lib
	install -d $(PREFIX)/include
	install -m 644 $(STATIC_LIB) $(PREFIX)/lib/
	install -m 755 $(SHARED_LIB) $(PREFIX)/lib/
	install -m 644 kjson.h $(PREFIX)/include/

.PHONY: all clean install test