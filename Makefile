NAME=cedro

CC=gcc -g -fshort-enums
CC_STRICT=$(CC) -std=c99 -fmax-errors=4 -pedantic-errors -Wall -Werror -Wsign-conversion -Wno-unused-function -Wno-unused-const-variable

all: release
debug:   build/$(NAME)-debug build/$(NAME)cc-debug
	if which valgrind; then valgrind --leak-check=yes build/$(NAME)-debug src/$(NAME).c >/dev/null; fi
	if which valgrind; then valgrind --leak-check=yes build/$(NAME)cc-debug src/$(NAME).c -I src -o /dev/null; fi
release: build/$(NAME) build/$(NAME)cc
.PHONY: all debug release

run: build/$(NAME)
	build/$(NAME) src/$(NAME).c
.PHONY: run

build/%cc-debug: src/%cc.c src/*.c src/*.h src/macros/*.h Makefile
	mkdir -p build
	$(CC_STRICT) -o $@ $<

build/%cc:       src/%cc.c src/*.c src/*.h src/macros/*.h Makefile
	mkdir -p build
	$(CC_STRICT) -o $@ $< -O

build/%-debug:   src/%.c src/*.c src/*.h src/macros/*.h Makefile
	mkdir -p build
	$(CC_STRICT) -o $@ $<

build/%:         src/%.c src/*.c src/*.h src/macros/*.h Makefile
	mkdir -p build
	$(CC_STRICT) -o $@ $< -O

doc:
	$(MAKE) -C doc
.PHONY: doc

test: src/$(NAME)-test.c build/$(NAME)
	$(CC_STRICT) -o build/$@ $<
	build/$@
.PHONY: test

# gcc -fanalyzer needs at least GCC 10.
# http://cppcheck.sourceforge.net/
# https://sparse.docs.kernel.org/en/latest/
check: build/$(NAME)
	mkdir -p doc/cppcheck
	cpp src/$(NAME).c | sed 's/^\(\s*\)# /\1\/\/ # /' >build/$(NAME).i
	cppcheck build/$(NAME).i --std=c99 --enable=performance,portability --xml 2>&1 | cppcheck-htmlreport --report-dir=doc/cppcheck --source-dir=.
	sparse -Wall build/$(NAME).i
	gcc -fanalyzer -o /dev/null -std=c99 -pedantic-errors -Wall -Wno-unused-function -Wno-unused-const-variable src/$(NAME).c
	scan-build -o doc/clang build/$(NAME)
	valgrind --leak-check=yes build/$(NAME) examples/hello.c >/dev/null

clean:
	rm -rf build
	if [ -e doc ]; then $(MAKE) -C doc clean; fi
.PHONY: clean
