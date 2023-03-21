#define main main_original
#include "cedro.c"
#undef main

#define run_test(name) test_##name(); eprintln("OK: " #name)
#define eq(a, b) (a == b)

void test_number()
{
  Byte_p text = B("100");
  Byte_p cursor = number(text, text + strlen((const char *) text));
  assert(eq(cursor, text + strlen((const char *) text)) ||
         (eprintln("Failed to parse “%s”", (const char *) text), false));
}

void test_const()
{
  mut_Byte_array array = init_Byte_array(10);
  // Will fail:
  push_Byte_array(&array, '@');
  *get_mut_Byte_array(&array, 0) = 0;
  // Will fail:
  //*get_mut_Byte_array(&array, 0) = 0;
  //*(array->items) = 0;
  //array->items[0] = 0;
}

char* to_string_Byte_array(Byte_array_p array)
{
  char* string = malloc(array->len * sizeof(char) + 1);
  for (size_t i = 0; i < array->len; ++i) {
    string[i] = (char)*get_Byte_array(array, i);
  }
  string[array->len] = 0;
  return string;
}

void test_array()
{
  Byte_p text = B("En un lugar de La Mancha, de cuyo nombre no quiero acordarme, no ha mucho tiempo que vivía un hidalgo de los de lanza en astillero, adarga antigua, rocín flaco y galgo corredor.");
  mut_Byte_array array = init_Byte_array(10);
  size_t text_len = strlen((char*const) text);
  for (size_t i = 0; i < text_len; ++i) {
    push_Byte_array(&array, text[i]);
  }
  assert(eq(array.len, text_len) ||
         (eprintln("Wrong text length %zu ≠ %zu", array.len, text_len), false));

  char* text_rebuilt = to_string_Byte_array(&array);
  assert(str_eq((char*const) text, (char*const) text_rebuilt) ||
         (fputs(text_rebuilt, stderr), false));
  free(text_rebuilt);

  mut_Byte_array text_array = {
    strlen((char*const) text) + 1, strlen((char*const) text) + 1,
    (mut_Byte_p) text
  };
  Byte_array_slice slice = Byte_array_slice_from(&text_array, 25, 25 + 35);
  // Will fail:
  //slice.array_p = &array;
  //splice_Byte_array(&array, 0, 0, slice);

  splice_Byte_array(&array, 11, 0, NULL, slice);
  assert(eq(214, array.len) ||
         (text_rebuilt = to_string_Byte_array(&array),
          eprintln("Insert:\n%s", text_rebuilt),
          free(text_rebuilt), false));

  splice_Byte_array(&array, 25 + 35, 36, NULL, (Byte_array_slice){0});
  assert(eq(178, array.len) ||
         (text_rebuilt = to_string_Byte_array(&array),
          eprintln("Delete:\n%s", text_rebuilt),
          free(text_rebuilt), false));

  splice_Byte_array(&array, 101, 76, NULL, slice);
  text_rebuilt = to_string_Byte_array(&array);
  assert(eq(137, array.len) ||
         (eprintln("Splice:\n%s", text_rebuilt), false));
  assert(str_eq(text_rebuilt, "En un lugar de cuyo nombre no quiero acordarme de La Mancha, no ha mucho tiempo que vivía un hidalgo de cuyo nombre no quiero acordarme.") ||
         (eprintln("Splice:\n%s", text_rebuilt), false));
  free(text_rebuilt);
}

int main(int argc, char** argv)
{
  run_test(array);
  run_test(const);

  run_test(number);
}
