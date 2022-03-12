.PHONY: default release debug static bt help doc clean
default: release
all: debug release

CMDNAME={template}
NAME={Template}
LOGO=logo.png
CSS=doc/api.css

# The include paths must be put also in Doxyfile (see below).

# Strict compilation flags, use during development:
CFLAGS=-g -L build/lib -I lib -I src -std=c99 -fmax-errors=4 -pedantic-errors -Wall -Werror -Wno-unused-function -Wno-unused-const-variable -Wsign-conversion

# Loose compilation flags, use for releases:
CFLAGS=-g -DNDEBUG -L build/lib -I lib -I src -std=c99

TEST_ARGUMENTS="dummy"

CEDRO=tools/cedro/cedro
CEDROCC=$(CEDRO)cc

LIBRARIES=lib/build/btree.c-0.2.1/btree.o

# include Makefile.libuv.mk

release: $(LIBRARIES) build/$(CMDNAME)
debug:   $(LIBRARIES) build/$(CMDNAME)-debug
static:  $(LIBRARIES) build/$(CMDNAME)-static
bt: debug
	gdb build/$(CMDNAME)-debug -batch -ex "run $(TEST_ARGUMENTS)" -ex "bt"
help:
	@echo "Compilation results go in 'build/'."
	@echo " release: optimized build, dynamic linking. (default)"
	@echo " debug: unoptimized build, dynamic linking."
	@echo " static:  optimized build,  static linking."
	@echo " doc: make documentation in 'doc/index.html', needs doxygen: https//doxygen.org"
	@echo " clean: remove build/, doc/, $(CEDRO), $(CEDROCC)"
.PHONY: release debug static bt help

$(CEDRO): $(CEDRO).c
	$(CC) $(CFLAGS) -O -o $@ $<
$(CEDROCC): $(CEDROCC).c $(CEDRO)
	$(CEDRO) --insert-line-directives $< | $(CC) $(CFLAGS) -I `dirname $(CEDRO)` -O -o $@ -x c -

build/$(CMDNAME).o:     src/main.c src/* Makefile $(CEDROCC)
	@mkdir -p build
	$(CEDROCC) $< $(CFLAGS) -c           -o $@ -O
build/$(CMDNAME):       build/$(CMDNAME).o $(LIBRARIES) Makefile
	$(CC)    $@.o $(CFLAGS) $(LIBRARIES) -o $@
	@if which valgrind >/dev/null; then CMD="valgrind --leak-check=yes --quiet $@ $(TEST_ARGUMENTS)"; if $$CMD </dev/null >/dev/null 2>/dev/null; then echo Valgrind check passed: $@; else echo Valgrind check failed: $@; echo Run check with:; echo $$CMD; fi; fi
	@echo '--->' $@

build/$(CMDNAME)-debug.o: src/main.c src/* Makefile $(CEDROCC)
	@mkdir -p build
	$(CEDROCC) $< $(CFLAGS) -c           -o $@
build/$(CMDNAME)-debug:   build/$(CMDNAME)-debug.o $(LIBRARIES) Makefile
	$(CC)    $@.o $(CFLAGS) $(LIBRARIES) -o $@
	@if which valgrind >/dev/null; then CMD="valgrind --leak-check=yes --quiet $@ $(TEST_ARGUMENTS)"; if $$CMD </dev/null >/dev/null 2>/dev/null; then echo Valgrind check passed: $@; else echo Valgrind check failed: $@; echo Run check with:; echo $$CMD; fi; fi
	@echo '--->' $@

build/$(CMDNAME)-static: src/main.c src/* $(LIBRARIES) Makefile $(CEDROCC)
	@mkdir -p build
	$(CEDROCC) $< $(CFLAGS) -o $@ -O -static
	@if which valgrind >/dev/null; then CMD="valgrind --leak-check=yes --quiet $@ $(TEST_ARGUMENTS)"; if $$CMD </dev/null >/dev/null 2>/dev/null; then echo Valgrind check passed: $@; else echo Valgrind check failed: $@; echo Run check with:; echo $$CMD; fi; fi
	@echo '--->' $@

lib/build/%.o: lib/%.c lib/%.h
	mkdir -p $$(dirname $@)
	$(CC) -c $< $(CFLAGS) -o $@ -O

doc: doc/api/main.c Makefile Doxyfile
	mkdir -p doc/api
	NAME="$(NAME)" doxygen

# Amalgamation of src/main.c
doc/api/main.c: src/* $(CEDROCC)
	mkdir -p doc/api
	CEDRO_CC='' $(CEDROCC) src/main.c >$@

Doxyfile:
	doxygen -g
	sed --in-place 's:^\(PROJECT_NAME\s\+\)=.*:\1= $$(NAME):g' Doxyfile
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

clean:
	rm -rf build lib/build doc/api
	rm -f $(CEDRO) $(CEDRO)cc
