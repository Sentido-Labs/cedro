CC=gcc
CC_STRICT=$(CC) -std=c99 -pedantic -Wall
NAME=cedro

all: $(NAME)
.PHONY: all

run: $(NAME)
	./$(NAME) $(NAME).c
.PHONY: run

$(NAME): $(NAME).c macros.h Makefile
	$(CC_STRICT) -o $@ $<

doc:
	$(MAKE) -C doc
.PHONY: doc

clean:
	rm -f $(NAME)
	$(MAKE) -C doc clean
