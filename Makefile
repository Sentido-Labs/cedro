NAME=cedro

CFLAGS=-g -fshort-enums -std=c99 -fmax-errors=4 -pedantic-errors -Wall -Werror -Wsign-conversion -Wno-unused-function -Wno-unused-const-variable

default: release
all: release debug
debug:   bin/$(NAME)-debug bin/$(NAME)cc-debug bin/$(NAME)-new-debug
release: bin/$(NAME)       bin/$(NAME)cc       bin/$(NAME)-new
.PHONY: default all debug release

bin/cedro-debug: src/cedro.c src/*.c src/*.h src/macros/*.h Makefile
	@mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $<
	@if which valgrind >/dev/null; then if valgrind --leak-check=yes --quiet bin/$(NAME) src/$(NAME)cc.c >/dev/null; then echo Valgrind check passed: $(NAME); fi; fi
bin/cedro:       src/cedro.c src/*.c src/*.h src/macros/*.h Makefile
	@mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $< -O
	@if which valgrind >/dev/null; then if valgrind --leak-check=yes --quiet bin/$(NAME) src/$(NAME)cc.c >/dev/null; then echo Valgrind check passed: $@; fi; fi

bin/cedrocc-debug: src/cedrocc.c Makefile bin/cedro-debug
	@mkdir -p bin
	bin/cedro --insert-line-directives $< | $(CC) $(CFLAGS) -I src -x c - -o $@
	@if which valgrind >/dev/null; then if valgrind --leak-check=yes --quiet bin/$(NAME)cc src/$(NAME)cc.c -I src -o /dev/null; then echo Valgrind check passed: $@; fi; fi
bin/cedrocc:       src/cedrocc.c Makefile bin/cedro
	@mkdir -p bin
	bin/cedro --insert-line-directives $< | $(CC) $(CFLAGS) -I src -x c - -o $@ -O
	@if which valgrind >/dev/null; then if valgrind --leak-check=yes --quiet bin/$(NAME)cc src/$(NAME)cc.c -I src -o /dev/null; then echo Valgrind check passed: $@; fi; fi

bin/cedro-new-debug: src/cedro-new.c template.zip Makefile bin/cedrocc
	@mkdir -p bin
	bin/cedrocc $< -I src $(CFLAGS) -o $@
bin/cedro-new:       src/cedro-new.c template.zip Makefile bin/cedrocc
	@mkdir -p bin
	bin/cedrocc $< -I src $(CFLAGS) -o $@ -O

bin/%: src/%.c Makefile bin/cedrocc
	@mkdir -p bin
	bin/cedrocc $< -I src $(CFLAGS) -o $@ -O

%.zip: % %/** bin/zip-template
	bin/zip-template $@ $<

doc:
	$(MAKE) -C doc
.PHONY: doc

test: src/$(NAME)-test.c bin/$(NAME)
	$(CC) $(CFLAGS) -o bin/$@ $<
	bin/$@
.PHONY: test

# gcc -fanalyzer needs at least GCC 10.
# http://cppcheck.sourceforge.net/
# https://sparse.docs.kernel.org/en/latest/
check: bin/$(NAME)
	mkdir -p doc/cppcheck
	cpp src/$(NAME).c | sed 's/^\(\s*\)# /\1\/\/ # /' >bin/$(NAME).i
	sparse -Wall bin/$(NAME).i
	valgrind --leak-check=yes bin/$(NAME) src/$(NAME)cc.c >/dev/null
	@echo
	@echo This was useful when Cedro was simpler, but it has become way too slow:
	@echo '    cppcheck bin/$(NAME).i --std=c99 --enable=performance,portability --xml 2>&1 | cppcheck-htmlreport --report-dir=doc/cppcheck --source-dir=src'
	gcc -fanalyzer -o /dev/null -std=c99 -pedantic-errors -Wall -Wno-unused-function -Wno-unused-const-variable src/$(NAME).c
# Used to work:	scan-bin -o doc/clang bin/$(NAME)

clean:
	rm -rf bin
	if [ -e doc ]; then $(MAKE) -C doc clean; fi
.PHONY: clean
