NAME=Cedro
SRC=../src/cedro.c ../src/array.h
LOGO=cedro.png
CSS=cedro.css
DIR=api

doc: $(SRC) Makefile Doxyfile
	NAME="$(NAME)" doxygen
	@for f in $$(find $(DIR) -name "*.html"); do sed --in-place -E -e 's:([^ ])\\n[ ]{4,}:\1</div>\n<div class="line">:g' -e '/&rsquo;<br[ /]*>$$/{N;s:&rsquo;<br[ /]*>\n'"'"':'"'"'\\n'"'"':g}' -e 's:\\/\\/://:g' -e 's:\\\\:\\:g' -e 's:\\/\\\*:/*:g' -e 's:\\\*\\/:*/:g' "$${f}" -e 's#id="titlearea">#id="titlearea" style="position:relative"><a class="img" style="display:inline-block;position:absolute;top:0.5em;right:0.5em;font-size:1rem;" href="http://sentido-labs.com"><img style="width:5em;" src="../sentido-labs-mono.svg"></a>#'; done
	@echo >>$(DIR)/search/search.css ' @media (prefers-color-scheme: dark) { .SRPage { background:#333; color:#c92; } }'
.PHONY: doc

Doxyfile:
	doxygen -g
	sed --in-place 's:^\(PROJECT_NAME\s\+\)=.*:\1= "$$(NAME)":g' Doxyfile
	sed --in-place 's:^\(PROJECT_LOGO\s\+\)=.*:\1= "$(LOGO)":g' Doxyfile
	sed --in-place 's:^\(FULL_PATH_NAMES\s\+\)=\s*YES:\1= NO:g' Doxyfile
	sed --in-place 's:^\(OPTIMIZE_OUTPUT_FOR_C\s\+\)=\s*NO:\1= YES:g' Doxyfile
	sed --in-place 's:^\(EXTRACT_ALL\s\+\)=\s*NO:\1= YES:g' Doxyfile
	sed --in-place 's:^\(EXTRACT_STATIC\s\+\)=\s*NO:\1= YES:g' Doxyfile
	sed --in-place 's:^\(INPUT\s\+\)=\s*:\1= $(SRC):g' Doxyfile
	sed --in-place 's:^\(GENERATE_HTML\s\+\)=\s*NO:\1= YES:g' Doxyfile
	sed --in-place 's:^\(HTML_OUTPUT\s\+\)=.*:\1= $(DIR):g' Doxyfile
	sed --in-place 's:^\(HTML_EXTRA_STYLESHEET\s\+\)=.*:\1= "$(CSS)":g' Doxyfile
	sed --in-place 's:^\(GENERATE_LATEX\s\+\)=\s*YES:\1= NO:g' Doxyfile
	sed --in-place 's:^\(MACRO_EXPANSION\s\+\)=\s*NO:\1= YES:g' Doxyfile
	sed --in-place 's:^\(EXPAND_ONLY_PREDEF\s\+\)=\s*NO:\1= YES:g' Doxyfile
	sed --in-place 's:^\(EXPAND_AS_DEFINED\s\+\)=\s*:\1= DEFINE_ARRAY_OF TYPEDEF TYPEDEF_STRUCT:g' Doxyfile

clean:
	rm -rf $(DIR) clang cppcheck
