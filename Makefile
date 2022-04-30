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
CFLAGS=-g -std=c99
CFLAGS_MINIZ=-g -std=c99

# -DNDEBUG mutes the unused-variable warnings/errors.
OPTIMIZATION=-O -DNDEBUG

VALGRIND_CHECK=valgrind --error-exitcode=123 --leak-check=yes
TEST_ARGUMENTS=src/$(NAME)cc.c

debug:   bin/$(NAME)-debug bin/$(NAME)cc-debug bin/$(NAME)-new-debug
release: bin/$(NAME)       bin/$(NAME)cc       bin/$(NAME)-new
static:  bin/$(NAME)-static bin/$(NAME)cc-static bin/$(NAME)-new-static

bin/$(NAME)-debug: src/cedro.c src/*.c src/*.h src/macros/*.h Makefile
	@mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $<
	@if which valgrind >/dev/null; then CMD="$(VALGRIND_CHECK) --quiet $@ $(TEST_ARGUMENTS)"; if $$CMD </dev/null >/dev/null; then echo Valgrind check passed: $@; else echo Valgrind check failed: $@; echo Run check with: "$(VALGRIND_CHECK) $@ $(TEST_ARGUMENTS)"; fi; fi
bin/$(NAME):       src/cedro.c src/*.c src/*.h src/macros/*.h Makefile
	@mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $< $(OPTIMIZATION)
	@if which valgrind >/dev/null; then CMD="$(VALGRIND_CHECK) --quiet $@ $(TEST_ARGUMENTS)"; if $$CMD </dev/null >/dev/null; then echo Valgrind check passed: $@; else echo Valgrind check failed: $@; echo Run check with: "$(VALGRIND_CHECK) $@ $(TEST_ARGUMENTS)"; fi; fi

bin/$(NAME)cc-debug: src/cedrocc.c Makefile bin/$(NAME)-debug
	@mkdir -p bin
	bin/$(NAME)-debug --insert-line-directives $< | $(CC) $(CFLAGS) -I src -x c - -o $@
	@if which valgrind >/dev/null; then CMD="$(VALGRIND_CHECK) --quiet $@ -o /dev/null $(TEST_ARGUMENTS)"; if $$CMD </dev/null >/dev/null; then echo Valgrind check passed: $@; else echo Valgrind check failed: $@; echo Run check with: "$(VALGRIND_CHECK) $@ $(TEST_ARGUMENTS)"; fi; fi
bin/$(NAME)cc:       src/cedrocc.c Makefile bin/$(NAME)
	@mkdir -p bin
	bin/$(NAME)       --insert-line-directives $< | $(CC) $(CFLAGS) -I src -x c - -o $@  $(OPTIMIZATION)
	@if which valgrind >/dev/null; then CMD="$(VALGRIND_CHECK) --quiet $@ -o /dev/null $(TEST_ARGUMENTS)"; if $$CMD </dev/null >/dev/null; then echo Valgrind check passed: $@; else echo Valgrind check failed: $@; echo Run check with: "$(VALGRIND_CHECK) $@ $(TEST_ARGUMENTS)"; fi; fi

bin/$(NAME)-new-debug: src/cedro-new.c template.zip Makefile bin/$(NAME)cc-debug
	@mkdir -p bin
	bin/$(NAME)cc-debug $< -I src $(CFLAGS_MINIZ) -o $@
bin/$(NAME)-new:       src/cedro-new.c template.zip Makefile bin/$(NAME)cc
	@mkdir -p bin
	bin/$(NAME)cc       $< -I src $(CFLAGS_MINIZ) -o $@ $(OPTIMIZATION)

bin/$(NAME)-static: src/cedro.c src/*.c src/*.h src/macros/*.h Makefile
	@mkdir -p bin
	$(CC) $(CFLAGS) -static -o $@ $< $(OPTIMIZATION)
bin/$(NAME)cc-static: src/cedrocc.c Makefile bin/$(NAME)
	@mkdir -p bin
	bin/$(NAME)       --insert-line-directives $< | $(CC) $(CFLAGS) -static -I src -x c - -o $@  $(OPTIMIZATION)
bin/$(NAME)-new-static: src/cedro-new.c template.zip Makefile bin/$(NAME)cc
	@mkdir -p bin
	bin/$(NAME)cc -static $< -I src $(CFLAGS_MINIZ) -o $@ $(OPTIMIZATION)

bin/%: src/%.c Makefile bin/$(NAME)cc
	@mkdir -p bin
	bin/$(NAME)cc $< -I src $(CFLAGS_MINIZ) -o $@ $(OPTIMIZATION)

%.zip: % %/* %/*/* bin/zip-template
	bin/zip-template $@ $<

doc:
	$(MAKE) -C doc

test: src/$(NAME)-test.c test/* bin/$(NAME) bin/$(NAME)-debug
	@$(CC) $(CFLAGS) -o bin/$@ $<
	@bin/$@
	@for f in test/*.c; do echo -n "$${f} ... "; OPTS=""; if [ -z "$${f##*-line-directives*}" ]; then OPTS="--insert-line-directives"; fi; ERROR=$$(bin/$(NAME) $${OPTS} "$${f}" | bin/$(NAME) - $${OPTS} --validate="test/reference/$${f##test/}" 2>&1); if [ "$$ERROR" ]; then echo "ERROR"; echo "$${ERROR}"; exit 7; else echo "OK"; fi; done

# gcc -fanalyzer needs at least GCC 11. GCC 10 gives false positives.
# https://valgrind.org/
# https://sparse.docs.kernel.org/en/latest/
# http://cppcheck.sourceforge.net/  https://github.com/danmar/cppcheck
check: bin/$(NAME)
# The sed filter is to avoid “warning: directive in macro's argument list”.
	$(CPP) src/$(NAME).c | sed 's/^\(\s*\)# /\1\/\/ # /' >bin/$(NAME).i
	@if which sparse >/dev/null; then echo 'Checking with sparse...'; sparse -Wall bin/$(NAME).i; else echo 'sparse not installed.'; fi
	@if which valgrind >/dev/null; then echo 'Checking with Valgrind...'; valgrind --leak-check=yes bin/$(NAME) src/$(NAME)cc.c >/dev/null; else echo 'valgrind not installed.'; fi
	@echo
	@echo 'Omitted because Clang 12 gives several false positives:'
	@echo "if which clang >/dev/null; then echo 'Checking with Clang...'; clang --analyze src/$(NAME).c; else echo 'clang not installed.'; fi"
	@echo
	@echo 'GCC should be version 11.2 or later because older releases such as 10.2 produce false positives on Cedro.'
	@if which gcc >/dev/null; then echo 'Checking with gcc '`gcc -dumpversion`'...'; gcc -fanalyzer -c -o /dev/null -std=c99 src/$(NAME).c; fi
	@echo
	@echo 'Omitted because it has become way too slow (around 25 minutes):'
	@echo "@if which cppcheck >/dev/null; then echo 'Checking with Cpphheck...'; cppcheck src/$(NAME).c --std=c99 --report-progress --enable=performance,portability; else echo 'cppcheck not installed.'; fi"

clean:
	rm -rf bin
	if [ -e doc ]; then $(MAKE) -C doc clean; fi
