#include <stdio.h>
#include <stdlib.h>

#pragma Cedro 1.0 defer

int main(int argc, char* argv[])
{
  int x = 0, y = 0;
  char *nivel1 = malloc(1);
  defer free(nivel1);

  for (x = 0; siguiente_x: x < 100; ++x) {
    char *nivel2 = malloc(2);
    defer free(nivel2);
    for (y = 0; y < 100; ++y) {
      char *nivel3 = malloc(3);
      defer free(nivel3);
      switch (x + y) {
        case 157:
          break encontrada_descomposición_del_número;
        case 11:
          x = 37;
          fprintf(stderr, "Saltamos de x=11 a x=%d\n", x);
          --x;
          continue siguiente_x;
      }
    }
  } encontrada_descomposición_del_número:

  if (x < 100 || y < 100) {
    fprintf(stderr, "Encontrada %d = %d + %d\n",
            x + y, x, y);
  }

  return 0;
}
