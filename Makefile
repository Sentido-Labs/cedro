NAME=cedro

CC=gcc -g
CC_STRICT=$(CC) -std=c99 -fmax-errors=4 -pedantic-errors -Wall -Wno-unused-function -Wno-unused-const-variable

all: debug release
	if which valgrind; then valgrind --leak-check=yes ./$(NAME) $(NAME).c; fi
debug:   $(NAME)-debug
release: $(NAME)
.PHONY: all debug release

run: $(NAME)
	./$(NAME) $(NAME).c
.PHONY: run

$(NAME)-debug: $(NAME).c defer.h array.h macros.h macros/*.h Makefile
	$(CC_STRICT) -o $@ $<

$(NAME):       $(NAME).c defer.h array.h macros.h macros/*.h Makefile
	$(CC_STRICT) -o $@ $< -O

doc:
	$(MAKE) -C doc
.PHONY: doc

test: $(NAME)-test.c $(NAME)
	$(CC_STRICT) -o $@ $<
	./test
.PHONY: test

# gcc -fanalyzer needs at least GCC 10.
# http://cppcheck.sourceforge.net/
# https://sparse.docs.kernel.org/en/latest/
check: $(NAME)
	mkdir -p doc/cppcheck
	cpp $(NAME).c | sed 's/^\(\s*\)# /\1\/\/ # /' >$(NAME).i
	cppcheck $(NAME).i --std=c99 --enable=performance,portability --xml 2>&1 | cppcheck-htmlreport --report-dir=doc/cppcheck --source-dir=.
	sparse $(NAME).i
	gcc -fanalyzer -o /dev/null -std=c99 -pedantic-errors -Wall -Wno-unused-function -Wno-unused-const-variable $(NAME).c
	scan-build -o doc/clang ./$(NAME)
	valgrind --leak-check=yes ./$(NAME) hello.c

clean:
	rm -f $(NAME) $(NAME)-debug
	$(MAKE) -C doc clean
.PHONY: clean
