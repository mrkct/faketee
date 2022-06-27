CC = gcc
CFLAGS = -Wall -Wextra -g -fsanitize=address

SOURCES = $(wildcard *.c)
PROGRAMS = $(SOURCES:.c=)

.PHONY = all clean

all: $(PROGRAMS)

clean:
	rm -f $(PROGRAMS)

%: %.c
	$(CC) $(CFLAGS) $< -o $@