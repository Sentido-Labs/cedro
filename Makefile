CC=gcc -g
CC_STRICT=$(CC) -g -std=c99 -pedantic-errors -Wall -Wno-unused-function -Wno-unused-const-variable
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

check: $(NAME).c macros/*.h Makefile
	mkdir -p doc/cppcheck
	cpp $(NAME).c >$(NAME).i
	cppcheck $(NAME).i --std=c99 --enable=performance,portability --xml 2>&1 | cppcheck-htmlreport --report-dir=doc/cppcheck --source-dir=.
	sparse $(NAME).i
	gcc -fanalyzer -o /dev/null -std=c99 -pedantic-errors -Wall -Wno-unused-function -Wno-unused-const-variable $(NAME).c

clean:
	rm -f $(NAME)
	$(MAKE) -C doc clean
