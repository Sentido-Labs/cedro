CC=gcc -g
CC_STRICT=$(CC) -g -std=c99 -pedantic-errors -Wall
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
