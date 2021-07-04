CMDNAME={template}
NAME={Template}
LOGO=logo.png
CSS=doc.css

# The include paths must be put also in Doxyfile (see below).
CFLAGS=-g -I lib/build -I lib -std=c99 -fmax-errors=4 -pedantic-errors -Wall -Werror -Wsign-conversion -Wno-unused-function -Wno-unused-const-variable
CEDRO=tools/cedro/cedro
CEDROCC=$(CEDRO)cc

# Some library files need to be processed with Cedro before compiling:
LIBRARIES=lib/build/hash-table.h

all: release
debug:   $(LIBRARIES) bin/$(CMDNAME)-debug
release: $(LIBRARIES) bin/$(CMDNAME)
.PHONY: all debug release

$(CEDRO): $(CEDRO).c
	$(CC) $(CFLAGS) -O -o $@ $<
$(CEDROCC): $(CEDROCC).c $(CEDRO)
	$(CEDRO) --insert-line-directives $< | $(CC) $(CFLAGS) -I `dirname $(CEDRO)` -O -o $@ -x c -

lib:
	$(MAKE) -C $@
.PHONY: lib

bin/$(CMDNAME)-debug: src/main.c src/*.c Makefile $(CEDROCC)
	@mkdir -p bin
	$(CEDROCC) $< $(CFLAGS) -o $@
	@if which valgrind >/dev/null; then if valgrind --leak-check=yes --quiet bin/$(CMDNAME) "dummy" </dev/null >/dev/null 2>/dev/null; then echo Valgrind check passed: $@; fi; fi

bin/$(CMDNAME):       src/main.c src/*.c Makefile $(CEDROCC)
	@mkdir -p bin
	$(CEDROCC) $< $(CFLAGS) -o $@ -O
	@if which valgrind >/dev/null; then if valgrind --leak-check=yes --quiet bin/$(CMDNAME) "dummy" </dev/null >/dev/null 2>/dev/null; then echo Valgrind check passed: $@; fi; fi

lib/build/%.h: lib/%.h $(CEDRO)
	@mkdir -p lib/build
	$(CEDRO) --insert-line-directives $< >$@
lib/build/%.c: lib/%.c $(CEDRO)
	@mkdir -p lib/build
	$(CEDRO) --insert-line-directives $< >$@

doc: src/* Makefile Doxyfile $(LIBRARIES)
	mkdir -p doc
	NAME="$(NAME)" doxygen
.PHONY: doc

Doxyfile:
	doxygen -g
	sed --in-place 's:^\(PROJECT_NAME\s\+\)=.*:\1= $$(NAME):g' Doxyfile
	sed --in-place 's:^\(PROJECT_LOGO\s\+\)=.*:\1= "$(LOGO)":g' Doxyfile
	sed --in-place 's:^\(FULL_PATH_NAMES\s\+\)=\s*YES:\1= NO:g' Doxyfile
	sed --in-place 's:^\(OPTIMIZE_OUTPUT_FOR_C\s\+\)=\s*NO:\1= YES:g' Doxyfile
	sed --in-place 's:^\(EXTRACT_ALL\s\+\)=\s*NO:\1= YES:g' Doxyfile
	sed --in-place 's:^\(EXTRACT_STATIC\s\+\)=\s*NO:\1= YES:g' Doxyfile
	sed --in-place 's:^\(INPUT\s\+\)=\s*:\1= src $(LIBRARIES):g' Doxyfile
	sed --in-place 's:^\(RECURSIVE\s\+\)=\s*NO:\1= YES:g' Doxyfile
	sed --in-place 's:^\(STRIP_CODE_COMMENTS\s\+\)=\s*YES:\1= NO:g' Doxyfile
	sed --in-place 's:^\(GENERATE_HTML\s\+\)=\s*NO:\1= YES:g' Doxyfile
	sed --in-place 's:^\(HTML_OUTPUT\s\+\)=.*:\1= doc:g' Doxyfile
	sed --in-place 's:^\(HTML_EXTRA_STYLESHEET\s\+\)=.*:\1= "$(CSS)":g' Doxyfile
	sed --in-place 's:^\(GENERATE_LATEX\s\+\)=\s*YES:\1= NO:g' Doxyfile
	sed --in-place 's:^\(MACRO_EXPANSION\s\+\)=\s*NO:\1= YES:g' Doxyfile
	sed --in-place 's:^\(EXPAND_ONLY_PREDEF\s\+\)=\s*NO:\1= YES:g' Doxyfile
	sed --in-place 's:^\(INCLUDE_PATH\s\+\)=\s*:\1= lib/build lib:g' Doxyfile
	sed --in-place 's:^\(EXPAND_AS_DEFINED\s\+\)=\s*:\1= DEFINE_HASH_SET_INT DEFINE_HASH_TABLE_INT DEFINE_HASH_SET_INT64 DEFINE_HASH_TABLE_INT64 DEFINE_HASH_SET_STR DEFINE_HASH_TABLE_STR:g' Doxyfile

clean:
	rm -rf bin lib/build doc
	make -C `dirname $(CEDRO)` clean
	make -C lib clean
.PHONY: clean
