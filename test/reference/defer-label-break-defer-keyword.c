#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[])
{
  int x = 0, y = 0;
  char *nivel1 = malloc(1);
  int x_inicial = 0;

busca_a_partir_de_x_inicial:
  for (x = x_inicial; x < 100; ++x) {
    char *nivel2 = malloc(2);
    for (y = 0; y < 100; ++y) {
      char *nivel3 = malloc(3);
      switch (x + y) {
        case 157:
          free(nivel3);
          free(nivel2);
          goto encontrada_descomposición_del_número;
        case 11:
          x_inicial = 37;
          fprintf(stderr, "Saltamos de x=11 a x=%d\n",
                  x_inicial);
          free(nivel3);
          free(nivel2);
          goto busca_a_partir_de_x_inicial;
      }
      free(nivel3);
    }
    free(nivel2);
  } encontrada_descomposición_del_número:

  if (x < 100 || y < 100) {
    fprintf(stderr, "Encontrada %d = %d + %d\n",
            x + y, x, y);
  }

  free(nivel1);

  return 0;
}
