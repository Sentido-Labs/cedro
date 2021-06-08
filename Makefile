NAME=cedro

CC=gcc -g -fshort-enums
CC_STRICT=$(CC) -std=c99 -fmax-errors=4 -pedantic-errors -Wall -Werror -Wsign-conversion -Wno-unused-function -Wno-unused-const-variable

all: release
debug:   $(NAME)-debug $(NAME)cc-debug
	if which valgrind; then valgrind --leak-check=yes ./$(NAME)-debug $(NAME).c >/dev/null; fi
release: $(NAME) $(NAME)cc
.PHONY: all debug release

run: $(NAME)
	./$(NAME) $(NAME).c
.PHONY: run

%cc-debug: %cc.c %.c array.h macros.h macros/*.h Makefile
	$(CC_STRICT) -o $@ $<

%cc:       %cc.c %.c array.h macros.h macros/*.h Makefile
	$(CC_STRICT) -o $@ $< -O

%-debug: %.c array.h macros.h macros/*.h Makefile
	$(CC_STRICT) -o $@ $<

%:       %.c array.h macros.h macros/*.h Makefile
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
	sparse -Wall $(NAME).i
	gcc -fanalyzer -o /dev/null -std=c99 -pedantic-errors -Wall -Wno-unused-function -Wno-unused-const-variable $(NAME).c
	scan-build -o doc/clang ./$(NAME)
	valgrind --leak-check=yes ./$(NAME) examples/hello.c >/dev/null

clean:
	rm -f $(NAME) $(NAME)-debug
	$(MAKE) -C doc clean
.PHONY: clean
