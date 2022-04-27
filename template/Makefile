.PHONY: default release debug static bt help help-es help-en doc check clean
default: release
all: debug release check

CMDNAME={#template}
NAME={#Template}
LOGO=logo.png
CSS=doc/api.css

# The include paths must be put also in Doxyfile (see below).

# Strict compilation flags, use during development:
CFLAGS=-g -I lib -I src -std=c99 -fmax-errors=4 -pedantic-errors -Wall -Werror -Wno-unused-function -Wno-unused-const-variable -Wsign-conversion

# Loose compilation flags, use for releases:
CFLAGS=-g -DNDEBUG -I lib -I src -std=c99

LDFLAGS=-L build/lib
LIBRARIES=lib/build/btree.c-0.2.1/btree.o

# This must go before the Makefile includes.
CEDRO_DIR=tools/cedro
CEDRO=$(CEDRO_DIR)/cedro
CEDROCC=$(CEDRO)cc

MAIN=src/main.c
# include Makefile.libuv.mk
# MAIN=src/main.libuv.c
# include Makefile.nanovg.mk
# MAIN=src/main.nanovg.c

TEST_ARGUMENTS="--exit-after-brief-delay"

release: $(LIBRARIES) build/$(CMDNAME)
debug:   $(LIBRARIES) build/$(CMDNAME)-debug
static:  $(LIBRARIES) build/$(CMDNAME)-static
# Get a backtrace from GDB:
bt: debug
	gdb build/$(CMDNAME)-debug -batch -ex "run $(TEST_ARGUMENTS)" -ex "bt"

HELP != case $$LANG in es*) echo help-es;; *) echo help-en;; esac
help: $(HELP)
help-es:
	@echo "Objetivos disponibles:"
	@echo "release: compilación optimizada, comprobaciones desactivadas, NDEBUG definido."
	@echo "  → build/$(CMDNAME)"
	@echo "debug:   compilación para diagnóstico, comprobaciones activadas."
	@echo "  → build/$(CMDNAME)-debug"
	@echo "static:  lo mismo que release, sólo que con enlace estático."
	@echo "  → build/$(CMDNAME)-static"
	@echo "doc:     construye la documentación con Doxygen https://www.doxygen.org"
	@echo "  → doc/api/index.html"
	@echo "check:   aplica varias herramientas de análisis estático:"
	@echo "  valgrind: https://valgrind.org/"
	@echo "  gcc -fanalyzer:"
	@echo "            https://gcc.gnu.org/onlinedocs/gcc/Static-Analyzer-Options.html"
	@echo "clean:   elimina el directorio bin/, y limpia también dentro de doc/."
help-en:
	@echo "Available targets:"
	@echo "release: optimized build, assertions disabled, NDEBUG defined."
	@echo "  → build/$(CMDNAME)"
	@echo "debug:   debugging build, assertions enabled."
	@echo "  → build/$(CMDNAME)-debug"
	@echo "static:  same as release, only statically linked."
	@echo "  → build/$(CMDNAME)-static"
	@echo "doc:     build documentation with Doxygen https://www.doxygen.org"
	@echo "  → doc/api/index.html"
	@echo "check:   apply several static analysis tools:"
	@echo "  valgrind: https://valgrind.org/"
	@echo "  gcc -fanalyzer:"
	@echo "            https://gcc.gnu.org/onlinedocs/gcc/Static-Analyzer-Options.html"
	@echo "clean:   remove bin/ directory, and clean also inside doc/."


$(CEDRO): $(CEDRO).c $(CEDRO_DIR)/macros/*
	$(CC) $(CFLAGS) -O -o $@ $<
$(CEDROCC): $(CEDROCC).c $(CEDRO)
	$(CEDRO) --insert-line-directives $< | $(CC) $(CFLAGS) -I `dirname $(CEDRO)` -O -o $@ -x c -


build/$(CMDNAME).o:     $(MAIN) src/* Makefile $(CEDROCC)
	@mkdir -p build
	$(CEDROCC) -c -o $@ $< $(CFLAGS) -O
build/$(CMDNAME):       build/$(CMDNAME).o $(LIBRARIES) Makefile
	$(CC)         -o $@ $@.o $(CFLAGS) $(LDFLAGS) $(LIBRARIES)
	@echo '--->' $@

build/$(CMDNAME)-debug.o: $(MAIN) src/* Makefile $(CEDROCC)
	@mkdir -p build
	$(CEDROCC) -c -o $@ $< $(CFLAGS)
build/$(CMDNAME)-debug:   build/$(CMDNAME)-debug.o $(LIBRARIES) Makefile
	$(CC)         -o $@ $@.o $(CFLAGS) $(LDFLAGS) $(LIBRARIES)
	@echo '--->' $@

build/$(CMDNAME)-static: $(MAIN) src/* $(LIBRARIES) Makefile $(CEDROCC)
	@mkdir -p build
	$(CEDROCC) -o $@ $< $(CFLAGS) $(LDFLAGS) $(LIBRARIES) -O -static
	@echo '--->' $@

lib/build/%.o: lib/%.c lib/%.h
	mkdir -p $$(dirname $@)
	$(CC) -c -o $@ $< $(CFLAGS) -O

doc: doc/api/main.c Makefile Doxyfile
	mkdir -p doc/api
	doxygen

# Amalgamation of src/main.c
doc/api/main.c: src/* $(CEDROCC)
	mkdir -p doc/api
	CEDRO_CC='' $(CEDROCC) $(MAIN) >$@

Doxyfile:
	doxygen -g
	sed --in-place 's:^\(PROJECT_NAME\s\+\)=.*:\1= "$(NAME)":g' Doxyfile
	sed --in-place 's:^\(PROJECT_LOGO\s\+\)=.*:\1= "$(LOGO)":g' Doxyfile
	sed --in-place 's:^\(FULL_PATH_NAMES\s\+\)=\s*YES:\1= NO:g' Doxyfile
	sed --in-place 's:^\(OPTIMIZE_OUTPUT_FOR_C\s\+\)=\s*NO:\1= YES:g' Doxyfile
	sed --in-place 's:^\(EXTRACT_ALL\s\+\)=\s*NO:\1= YES:g' Doxyfile
	sed --in-place 's:^\(EXTRACT_STATIC\s\+\)=\s*NO:\1= YES:g' Doxyfile
	sed --in-place 's:^\(INPUT\s\+\)=\s*:\1= doc/api/main.c lib:g' Doxyfile
	sed --in-place 's:^\(RECURSIVE\s\+\)=\s*NO:\1= YES:g' Doxyfile
	sed --in-place 's:^\(STRIP_CODE_COMMENTS\s\+\)=\s*YES:\1= NO:g' Doxyfile
	sed --in-place 's:^\(GENERATE_HTML\s\+\)=\s*NO:\1= YES:g' Doxyfile
	sed --in-place 's:^\(HTML_OUTPUT\s\+\)=.*:\1= doc/api:g' Doxyfile
	sed --in-place 's:^\(HTML_EXTRA_STYLESHEET\s\+\)=.*:\1= "$(CSS)":g' Doxyfile
	sed --in-place 's:^\(GENERATE_LATEX\s\+\)=\s*YES:\1= NO:g' Doxyfile
	sed --in-place 's:^\(MACRO_EXPANSION\s\+\)=\s*NO:\1= YES:g' Doxyfile
	sed --in-place 's:^\(EXPAND_ONLY_PREDEF\s\+\)=\s*NO:\1= YES:g' Doxyfile
	sed --in-place 's:^\(INCLUDE_PATH\s\+\)=\s*:\1= lib:g' Doxyfile
	sed --in-place 's:^\(EXPAND_AS_DEFINED\s\+\)=\s*:\1= DEFINE_HASH_SET_INT DEFINE_HASH_TABLE_INT DEFINE_HASH_SET_INT64 DEFINE_HASH_TABLE_INT64 DEFINE_HASH_SET_STR DEFINE_HASH_TABLE_STR:g' Doxyfile

# gcc -fanalyzer needs at least GCC 11. GCC 10 gives false positives.
# https://valgrind.org/
# https://sparse.docs.kernel.org/en/latest/
# http://cppcheck.sourceforge.net/  https://github.com/danmar/cppcheck
check: debug
# The sed filters are to:
# - convert escaped non-ASCII characters \uxxxx into uxxxx for Cppcheck.
# - avoid “warning: directive in macro's argument list”.
	CEDRO_CC='' $(CEDROCC) $(CFLAGS) $(MAIN) --cedro:escape-ucn | sed 's/\\\([uU]\)/\1/' | sed 's/^\(\s*\)# /\1\/\/ # /' >build/$(CMDNAME).i
	@if which valgrind >/dev/null; then echo 'Checking with Valgrind...'; valgrind --leak-check=yes build/$(CMDNAME)-debug $(TEST_ARGUMENTS) >/dev/null; else echo 'valgrind not installed.'; fi
	@echo
	@if which clang >/dev/null; then echo 'Checking with Clang...'; CEDRO_CC='' $(CEDROCC) $(CFLAGS) $(MAIN) | $(CEDRO) --escape-ucn | clang --analyze $(CFLAGS) -o /dev/null -x c -; else echo 'clang not installed.'; fi
	@echo
	@echo 'The analyzer in GCC 11 is much better than in 10, this is '`gcc -dumpversion`'.'
	@if which gcc >/dev/null; then echo 'Checking with gcc...'; CEDRO_CC='' $(CEDROCC) $(CFLAGS) $(MAIN) | gcc -fanalyzer -c -o /dev/null $(CFLAGS) -x c -; fi
	@echo
	@if which cppcheck >/dev/null; then cppcheck build/$(CMDNAME).i --std=c99 --report-progress --enable=performance,portability; else echo 'cppcheck not installed.'; fi

clean:
	rm -rf build lib/build doc/api
	rm -f $(CEDRO) $(CEDRO)cc
