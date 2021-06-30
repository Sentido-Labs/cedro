# {Template} 0.1a

Just an example of using Cedro together with some utility functions,
that you can use as base for new programs.

It includes my customized hash table and btree libraries derived from khash.

- [main](doc/main_8c.html)
- [hash-table](doc/hash-table_8h.html)
- [btree](doc/btree_8h.html)

## Build
`make` to build the executable as `bin/{template}`,
or `make doc` to build the API documentation
using [Doxygen](https://www.doxygen.nl/index.html).

The files `src/main.c` and `lib/hash-table.h` are processed with Cedro.

## License
TODO: put license here (also `src/main.cc`), for instance GPL or MIT.
You can get help picking one at: https://choosealicense.com/

The copy of Cedro under `tools/cedro/` has the same licence as Cedro,
but that does not affect your code if you do not include or link it
into your program.
Here it is used only to process the source code, and the result
is of course not a derivative work of Cedro.
