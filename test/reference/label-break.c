#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[])
{
  int x = 0, y = 0;

  for (x = 0; x < 100; ++x) {
    for (y = 0; y < 100; ++y) {
      switch (x + y) {
        case 157:
          goto encontrada_descomposición_del_número;
        case 11:
          x = 37;
          fprintf(stderr, "Saltamos de x=11 a x=%d\n", x);
          --x;
          goto siguiente_x;
      }
    }
  siguiente_x:;
  } encontrada_descomposición_del_número:

  if (x < 100 || y < 100) {
    fprintf(stderr, "Encontrada %d = %d + %d\n",
            x + y, x, y);
  }

  return 0;
}
