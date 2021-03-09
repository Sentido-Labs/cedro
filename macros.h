defmacro(count_markers, /* (mut_Marker_array_p markers, Buffer_p src) */ {
    fprintf(stderr, "Markers: %ld\n", markers->len);
  });

defmacro(function_pointer, /* (mut_Marker_array_p markers, Buffer_p src) */ {
    fprintf(stderr, "TODO: reorganize function_pointer(void fname(arg1, arg2)) as void (*fname)(arg1, arg2)\n");
  });

defmacro(fn, /* (mut_Marker_array_p markers, Buffer_p src) */ {
    fprintf(stderr, "TODO: reorganize fn fname(arg1: type1, arg2: mut type2) -> void {...} as void fname(const type1 arg1, type2 arg2) {...}\n");
  });

defmacro(let, /* (mut_Marker_array_p markers, Buffer_p src) */ {
    fprintf(stderr, "TODO: reorganize let x: int = 3; as int x = 3;\n");
  });
