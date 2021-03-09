CC=gcc -std=c99 -pedantic
NAME=cedro

all: $(NAME)

run: $(NAME)
	./$(NAME) $(NAME).c

$(NAME): $(NAME).c macros.h Makefile
	$(CC) -o $@ $<

doc:
	$(MAKE) -C doc
.PHONY: doc

clean:
	rm -f $(NAME)
	$(MAKE) -C doc clean
