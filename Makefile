NAME=cedro

CC=gcc -g -fshort-enums
CC_STRICT=$(CC) -std=c99 -fmax-errors=4 -pedantic-errors -Wall -Werror -Wsign-conversion -Wno-unused-function -Wno-unused-const-variable

all: release
debug:   bin/$(NAME)-debug bin/$(NAME)cc-debug
	if which valgrind; then valgrind --leak-check=yes bin/$(NAME)-debug src/$(NAME).c >/dev/null; fi
	if which valgrind; then valgrind --leak-check=yes bin/$(NAME)cc-debug src/$(NAME).c -I src -o /dev/null; fi
release: bin/$(NAME) bin/$(NAME)cc
.PHONY: all debug release

run: bin/$(NAME)
	bin/$(NAME) src/$(NAME).c
.PHONY: run

bin/%cc-debug: src/%cc.c bin/cedro-debug
	mkdir -p bin
	bin/cedro-debug --insert-line-directives $< | $(CC_STRICT) -I src -x c - -o $@

bin/%cc:       src/%cc.c bin/cedro
	mkdir -p bin
	bin/cedro       --insert-line-directives $< | $(CC_STRICT) -I src -x c - -o $@ -O

bin/%-debug:   src/%.c src/*.c src/*.h src/macros/*.h Makefile
	mkdir -p bin
	$(CC_STRICT) -o $@ $<

bin/%:         src/%.c src/*.c src/*.h src/macros/*.h Makefile
	mkdir -p bin
	$(CC_STRICT) -o $@ $< -O

doc:
	$(MAKE) -C doc
.PHONY: doc

test: src/$(NAME)-test.c bin/$(NAME)
	$(CC_STRICT) -o bin/$@ $<
	bin/$@
.PHONY: test

# gcc -fanalyzer needs at least GCC 10.
# http://cppcheck.sourceforge.net/
# https://sparse.docs.kernel.org/en/latest/
check: bin/$(NAME)
	mkdir -p doc/cppcheck
	cpp src/$(NAME).c | sed 's/^\(\s*\)# /\1\/\/ # /' >bin/$(NAME).i
	cppcheck bin/$(NAME).i --std=c99 --enable=performance,portability --xml 2>&1 | cppcheck-htmlreport --report-dir=doc/cppcheck --source-dir=.
	sparse -Wall bin/$(NAME).i
	gcc -fanalyzer -o /dev/null -std=c99 -pedantic-errors -Wall -Wno-unused-function -Wno-unused-const-variable src/$(NAME).c
	scan-bin -o doc/clang bin/$(NAME)
	valgrind --leak-check=yes bin/$(NAME) examples/hello.c >/dev/null

clean:
	rm -rf bin
	if [ -e doc ]; then $(MAKE) -C doc clean; fi
.PHONY: clean
