NAME=Cedro
SRC=../cedro.c ../array.h ../defer.h
LOGO=cedro.png
CSS=cedro.css

doc: ../*.c ../*.h Makefile Doxyfile
	doxygen
	sustituye --old='([^ ])\\n[ ]{4,}' --new='\1</div>\n<div class="line">' html/
	sustituye --old='\\/\\/' --new='//' html/
	sustituye --old='\\/\\\*' --new='/*' html/
	sustituye --old='\\\*\\/' --new='*/' html/

Doxyfile:
	doxygen -g
	sed --in-place 's:^\(PROJECT_NAME\s\+\)=.*:\1= "$(NAME)":g' Doxyfile
	sed --in-place 's:^\(PROJECT_LOGO\s\+\)=.*:\1= "$(LOGO)":g' Doxyfile
	sed --in-place 's:^\(FULL_PATH_NAMES\s\+\)=\s*YES:\1= NO:g' Doxyfile
	sed --in-place 's:^\(OPTIMIZE_OUTPUT_FOR_C\s\+\)=\s*NO:\1= YES:g' Doxyfile
	sed --in-place 's:^\(EXTRACT_ALL\s\+\)=\s*NO:\1= YES:g' Doxyfile
	sed --in-place 's:^\(EXTRACT_STATIC\s\+\)=\s*NO:\1= YES:g' Doxyfile
	sed --in-place 's:^\(INPUT\s\+\)=\s*:\1= $(SRC):g' Doxyfile
	sed --in-place 's:^\(GENERATE_HTML\s\+\)=\s*NO:\1= YES:g' Doxyfile
	sed --in-place 's:^\(HTML_EXTRA_STYLESHEET\s\+\)=.*:\1= "$(CSS)":g' Doxyfile
	sed --in-place 's:^\(GENERATE_LATEX\s\+\)=\s*YES:\1= NO:g' Doxyfile
	sed --in-place 's:^\(MACRO_EXPANSION\s\+\)=\s*NO:\1= YES:g' Doxyfile
	sed --in-place 's:^\(EXPAND_ONLY_PREDEF\s\+\)=\s*NO:\1= YES:g' Doxyfile
	sed --in-place 's:^\(EXPAND_AS_DEFINED\s\+\)=\s*:\1= DEFINE_ARRAY_OF TYPEDEF TYPEDEF_STRUCT:g' Doxyfile

clean:
	rm -rf html latex tree-sitter/html tree-sitter/latex
