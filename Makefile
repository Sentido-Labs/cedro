.PHONY: default release debug help help-es help-en doc test check clean
default: release
all: release debug

HELP != case $$LANG in es*) echo help-es;; *) echo help-en;; esac
help: $(HELP)
help-es:
	@echo "Objetivos disponibles:"
	@echo "release: compilación optimizada, comprobaciones desactivadas, NDEBUG definido."
	@echo "  → bin/cedro*"
	@echo "debug:   compilación para diagnóstico, comprobaciones activadas."
	@echo "  → bin/cedro*-debug"
	@echo "static:  lo mismo que release, sólo que con enlace estático."
	@echo "  → bin/cedro*-static"
	@echo "doc:     construye la documentación con Doxygen https://www.doxygen.org"
	@echo "  → doc/api/index.html"
	@echo "test:    construye tanto release como debug, y ejecuta la batería de pruebas."
	@echo "check:   aplica varias herramientas de análisis estático:"
	@echo "  sparse:   https://sparse.docs.kernel.org/"
	@echo "  valgrind: https://valgrind.org/"
	@echo "  gcc -fanalyzer:"
	@echo "            https://gcc.gnu.org/onlinedocs/gcc/Static-Analyzer-Options.html"
	@echo "clean:   elimina el directorio bin/, y limpia también dentro de doc/."
help-en:
	@echo "Available targets:"
	@echo "release: optimized build, assertions disabled, NDEBUG defined."
	@echo "  → bin/cedro*"
	@echo "debug:   debugging build, assertions enabled."
	@echo "  → bin/cedro*-debug"
	@echo "static:  same as release, only statically linked."
	@echo "  → bin/cedro*-static"
	@echo "doc:     build documentation with Doxygen https://www.doxygen.org"
	@echo "  → doc/api/index.html"
	@echo "test:    build both release and debug, then run test suite."
	@echo "check:   apply several static analysis tools:"
	@echo "  sparse:   https://sparse.docs.kernel.org/"
	@echo "  valgrind: https://valgrind.org/"
	@echo "  gcc -fanalyzer:"
	@echo "            https://gcc.gnu.org/onlinedocs/gcc/Static-Analyzer-Options.html"
	@echo "clean:   remove bin/ directory, and clean also inside doc/."

NAME=cedro

# Strict compilation flags, use during development:
CFLAGS=-g -fshort-enums -std=c99 -fmax-errors=4 -pedantic-errors -Wall -Werror -Wno-unused-function -Wno-unused-const-variable -Wsign-conversion
# miniz has many implicit sign conversions, after removing some it works on
# some manchines but it is not enough in others, so better ignore it:
CFLAGS_MINIZ=-g -fshort-enums -std=c99 -fmax-errors=4 -pedantic-errors -Wall -Werror -Wno-unused-function -Wno-unused-const-variable

# Loose compilation flags, use for releases:
#CFLAGS=-g -std=c99
#CFLAGS_MINIZ=-g -std=c99

# -DNDEBUG mutes the unused-variable warnings/errors.
OPTIMIZATION=-O -DNDEBUG

VALGRIND_CHECK=valgrind --error-exitcode=123 --leak-check=yes
TEST_ARGUMENTS=src/$(NAME)cc.c

debug:   bin/$(NAME)-debug bin/$(NAME)cc-debug bin/$(NAME)-new-debug
release: bin/$(NAME)       bin/$(NAME)cc       bin/$(NAME)-new
static:  bin/$(NAME)-static bin/$(NAME)cc-static bin/$(NAME)-new-static

bin/cedro-debug: src/cedro.c src/*.c src/*.h src/macros/*.h Makefile
	@mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $<
	@if which valgrind >/dev/null; then CMD="$(VALGRIND_CHECK) --quiet $@ $(TEST_ARGUMENTS)"; if $$CMD </dev/null >/dev/null; then echo Valgrind check passed: $@; else echo Valgrind check failed: $@; echo Run check with: "$(VALGRIND_CHECK) $@ $(TEST_ARGUMENTS)"; fi; fi
bin/cedro:       src/cedro.c src/*.c src/*.h src/macros/*.h Makefile
	@mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $< $(OPTIMIZATION)
	@if which valgrind >/dev/null; then CMD="$(VALGRIND_CHECK) --quiet $@ $(TEST_ARGUMENTS)"; if $$CMD </dev/null >/dev/null; then echo Valgrind check passed: $@; else echo Valgrind check failed: $@; echo Run check with: "$(VALGRIND_CHECK) $@ $(TEST_ARGUMENTS)"; fi; fi

bin/cedrocc-debug: src/cedrocc.c Makefile bin/cedro-debug
	@mkdir -p bin
	bin/cedro-debug --insert-line-directives $< | $(CC) $(CFLAGS) -I src -x c - -o $@
	@if which valgrind >/dev/null; then CMD="$(VALGRIND_CHECK) --quiet $@ $(TEST_ARGUMENTS)"; if $$CMD </dev/null >/dev/null; then echo Valgrind check passed: $@; else echo Valgrind check failed: $@; echo Run check with: "$(VALGRIND_CHECK) $@ $(TEST_ARGUMENTS)"; fi; fi
bin/cedrocc:       src/cedrocc.c Makefile bin/cedro
	@mkdir -p bin
	bin/cedro       --insert-line-directives $< | $(CC) $(CFLAGS) -I src -x c - -o $@  $(OPTIMIZATION)
	@if which valgrind >/dev/null; then CMD="$(VALGRIND_CHECK) --quiet $@ $(TEST_ARGUMENTS)"; if $$CMD </dev/null >/dev/null; then echo Valgrind check passed: $@; else echo Valgrind check failed: $@; echo Run check with: "$(VALGRIND_CHECK) $@ $(TEST_ARGUMENTS)"; fi; fi

bin/cedro-new-debug: src/cedro-new.c template.zip Makefile bin/cedrocc-debug
	@mkdir -p bin
	bin/cedrocc-debug $< -I src $(CFLAGS_MINIZ) -o $@
bin/cedro-new:       src/cedro-new.c template.zip Makefile bin/cedrocc
	@mkdir -p bin
	bin/cedrocc       $< -I src $(CFLAGS_MINIZ) -o $@ $(OPTIMIZATION)

bin/cedro-static: src/cedro.c src/*.c src/*.h src/macros/*.h Makefile
	@mkdir -p bin
	$(CC) $(CFLAGS) -static -o $@ $< $(OPTIMIZATION)
bin/cedrocc-static: src/cedrocc.c Makefile bin/cedro
	@mkdir -p bin
	bin/cedro       --insert-line-directives $< | $(CC) $(CFLAGS) -static -I src -x c - -o $@  $(OPTIMIZATION)
bin/cedro-new-static: src/cedro-new.c template.zip Makefile bin/cedrocc
	@mkdir -p bin
	bin/cedrocc -static $< -I src $(CFLAGS_MINIZ) -o $@ $(OPTIMIZATION)

bin/%: src/%.c Makefile bin/cedrocc
	@mkdir -p bin
	bin/cedrocc $< -I src $(CFLAGS_MINIZ) -o $@ $(OPTIMIZATION)

%.zip: % %/** bin/zip-template template/*
	bin/zip-template $@ $<

doc:
	$(MAKE) -C doc

test: src/$(NAME)-test.c test/* bin/$(NAME) bin/$(NAME)-debug
	@$(CC) $(CFLAGS) -o bin/$@ $<
	@bin/$@
	@for f in test/*.c; do echo -n "$${f} ... "; OPTS=""; if [ -z "$${f##*-line-directives*}" ]; then OPTS="--insert-line-directives"; fi; ERROR=$$(bin/cedro $${OPTS} "$${f}" | bin/cedro - $${OPTS} --validate="test/reference/$${f##test/}" 2>&1); if [ "$$ERROR" ]; then echo "ERROR"; echo "$${ERROR}"; exit 7; else echo "OK"; fi; done

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
	@echo ''
	gcc -fanalyzer -o /dev/null -std=c99 -pedantic-errors -Wall -Wno-unused-function -Wno-unused-const-variable src/$(NAME).c
# Used to work:	scan-bin -o doc/clang bin/$(NAME)

clean:
	rm -rf bin
	if [ -e doc ]; then $(MAKE) -C doc clean; fi
