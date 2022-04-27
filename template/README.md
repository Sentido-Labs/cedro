# {#Template} 0.1a

Just an example of using Cedro together with some utility functions,
that you can use as base for new programs.

The template includes several project drafts, that can be activated
in the generated Makefile:

- The default is a command line (“CLI”) tool that identifies the type of each argument, and uses a btree to count how many times each one is repeated. This gets built if you do not modify the Makefile.
  It uses a customized hash table library derived from
[khash](http://attractivechaos.github.io/klib/#About),
and also Josh Baker’s [btree.c](https://github.com/tidwall/btree.c).
  - [main](doc/api/main_8c.html)
  - [hash-table](doc/api/hash-table_8h.html)
  - [btree](doc/api/btree_8c.html)

- Graphical application with nanovg. Downloads (with curl) and compiles automatically nanovg, GLFW, and GLEW.

    include Makefile.nanovg.mk
    MAIN=src/main.nanovg.c

- HTTP/1.1 server using libuv. Downloads (with curl) and compiles automatically libuv.

    include Makefile.libuv.mk
    MAIN=src/main.libuv.c

## Build
`make` to build the executable as `bin/{#template}`,
and `make doc` to build the API documentation
using [Doxygen](https://www.doxygen.nl/index.html).

## License
TODO: put license here (also `src/main.cc`), for instance GPL or MIT.
You can get help picking one at: https://choosealicense.com/

The copy of Cedro under `tools/cedro/` is under the Apache License 2.0,
and anyway it does not affect your code if you do not include or link it
into your program.
Here it is used only to process the source code, and the result
is of course not a derivative work of Cedro.
