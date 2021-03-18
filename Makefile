CC=gcc -g
#ANALYZE=-fanalyzer
CC_STRICT=$(CC) -g -std=c99 -pedantic-errors -Wall $(ANALYZE)
NAME=cedro

all: $(NAME)
.PHONY: all

run: $(NAME)
	./$(NAME) $(NAME).c
.PHONY: run

$(NAME): $(NAME).c macros/*.h Makefile
	$(CC_STRICT) -o $@ $<

doc:
	$(MAKE) -C doc
.PHONY: doc

test: $(NAME)-test.c $(NAME) macros/*.h Makefile
	$(CC_STRICT) -o $@ $<
	./test
.PHONY: test

clean:
	rm -f $(NAME)
	$(MAKE) -C doc clean
