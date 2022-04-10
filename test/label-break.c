#include <stdio.h>
#include <stdlib.h>

#pragma Cedro 1.0

int main(int argc, char* argv[])
{
  int x = 0, y = 0;
  int x_inicial = 0;

busca_a_partir_de_x_inicial:
  for (x = x_inicial; x < 100; ++x) {
    for (y = 0; y < 100; ++y) {
      switch (x + y) {
        case 157:
          break encontrada_descomposición_del_número;
        case 11:
          x_inicial = 37;
          fprintf(stderr, "Saltamos de x=11 a x=%d\n",
                  x_inicial);
          continue busca_a_partir_de_x_inicial;
      }
    }
  } encontrada_descomposición_del_número:

  if (x < 100 || y < 100) {
    fprintf(stderr, "Encontrada %d = %d + %d\n",
            x + y, x, y);
  }

  return 0;
}
