#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[])
{
  int x = 0, y = 0;
  char *nivel1 = malloc(1);

  for (x = 0; x < 100; ++x) {
    char *nivel2 = malloc(2);
    for (y = 0; y < 100; ++y) {
      char *nivel3 = malloc(3);
      switch (x + y) {
        case 157:
          free(nivel3);
          free(nivel2);
          goto encontrada_descomposición_del_número;
        case 11:
          x = 37;
          fprintf(stderr, "Saltamos de x=11 a x=%d\n", x);
          --x;
          free(nivel3);
          goto siguiente_x;
      }
      free(nivel3);
    }
  siguiente_x:
    free(nivel2);
  } encontrada_descomposición_del_número:

  if (x < 100 || y < 100) {
    fprintf(stderr, "Encontrada %d = %d + %d\n",
            x + y, x, y);
  }

  free(nivel1);

  return 0;
}
