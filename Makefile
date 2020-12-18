CC=gcc -std=c99
NAME=ccx

all: run

run: $(NAME)
	./$(NAME) $(NAME).c

$(NAME): $(NAME).c macros.h Makefile
	$(CC) -o $@ $<

docs: *.c *.h Makefile Doxyfile
	doxygen 

Doxyfile:
	doxygen -g
	sed --in-place 's/^\(PROJECT_NAME\s\+\)=.*/\1= "$(NAME)"/g' Doxyfile
	sed --in-place 's/^\(INPUT\s\+\)=\s*/\1= $(NAME).c/g' Doxyfile
	sed --in-place 's/^\(OUTPUT_DIRECTORY\s\+\)=\s*/\1= docs/g' Doxyfile
	sed --in-place 's/^\(EXTRACT_ALL\s\+\)=\s*NO/\1= YES/g' Doxyfile
	sed --in-place 's/^\(GENERATE_LATEX\s\+\)=.*/\1= NO/g' Doxyfile

clean:
	rm -f $(NAME)
