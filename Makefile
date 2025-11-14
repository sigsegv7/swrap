CFLAGS = -pedantic -Wall
CC = clang

.PHONY: all
all: swrap
	$(CC) swrap.c -o $@
