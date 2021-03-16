#define main main_original
#include "cedro.c"
#undef main

#define run_test(name) test_##name(); log("OK: " #name)

void test_number()
{
  Byte_p text = B("100");
  Byte_p cursor = number(text, text + strlen((const char *) text));
  assert(cursor == text + strlen((const char *) text));
}

void test_const()
{
  mut_Byte_array array;
  init_Byte_array(&array, 10);
  // Will fail:
  // push_Byte_array(&array, '@');
  push_Byte_array(&array, B("@"));
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
    string[i] = *get_Byte_array(array, i);
  }
  string[array->len] = 0;
  return string;
}

void test_array()
{
  Byte_p text = B("En un lugar de La Mancha, de cuyo nombre no quiero acordarme, no ha mucho tiempo que vivía un hidalgo de los de lanza en astillero, adarga antigua, rocín flaco y galgo corredor.");
  log((char*const) text);
  mut_Byte_array array;
  init_Byte_array(&array, 10);
  size_t text_len = strlen((char*const) text);
  for (size_t i = 0; i < text_len; ++i) {
    push_Byte_array(&array, text + i);
  }
  assert(array.len == text_len);

  char* text_rebuilt = to_string_Byte_array(&array);
  log(text_rebuilt);
  assert(0 == strcmp((char*const) text, (char*const) text_rebuilt));
  free(text_rebuilt);

  mut_Byte_array text_array = {
    strlen((char*const) text) + 1, strlen((char*const) text) + 1,
    (mut_Byte_p) text
  };
  mut_Byte_array_slice slice;// = { &text_array 25, 25 + 35,  };
  init_Byte_array_slice(&slice, &text_array, 25, 25 + 35);
  // Will fail:
  //slice.array_p = &array;
  //splice_Byte_array(&array, 0, 0, &slice);

  splice_Byte_array(&array, 11, 0, &slice);
  assert(214 == array.len);
  text_rebuilt = to_string_Byte_array(&array);
  log("Insert:\n%s", text_rebuilt);
  free(text_rebuilt);

  splice_Byte_array(&array, 25 + 35, 36, NULL);
  assert(178 == array.len);
  text_rebuilt = to_string_Byte_array(&array);
  log("Delete:\n%s", text_rebuilt);
  free(text_rebuilt);

  splice_Byte_array(&array, 101, 76, &slice);
  assert(137 == array.len);
  text_rebuilt = to_string_Byte_array(&array);
  log("Splice:\n%s", text_rebuilt);
  assert(str_eq(text_rebuilt, "En un lugar de cuyo nombre no quiero acordarme de La Mancha, no ha mucho tiempo que vivía un hidalgo de cuyo nombre no quiero acordarme."));
  free(text_rebuilt);
}

int main(int argc, char** argv)
{
  // Enable core dumps on assert() failures:
  struct rlimit core_limit = { RLIM_INFINITY, RLIM_INFINITY };
  assert(setrlimit(RLIMIT_CORE, &core_limit) == 0);

  run_test(array);
  run_test(const);

  run_test(number);
}
