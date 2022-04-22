#include <stdio.h>

#pragma Cedro 1.0

typedef struct {
  const char* a;
  const char* b;
} char_slice_t;
const char* texto = "uno dos tres";

/** Extrae "dos" de `texto`. */
int main(void)
{
  char_slice_t porción = texto[4..+3];
  const char* cursor;
  for (cursor = porción.a; cursor != porción.b; ++cursor) {
    putc(*cursor, stderr);
  }
  putc('\n', stderr);
}
