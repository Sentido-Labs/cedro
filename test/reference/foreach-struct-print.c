#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

void imprime_double(double n, FILE* salida)
{
  fprintf(salida, "%f", n);
}

typedef uint32_t Color_ARGB;
void imprime_Color_ARGB(Color_ARGB c, FILE* salida)
{
  if ((c & 0xFF000000) == 0xFF000000) {
    fprintf(salida, "#%.6X", c & 0x00FFFFFF);
  } else {
    fprintf(salida, "#%.8X", c);
  }
}

typedef struct Punto {
  double x; /** Posición X. */
  double y; /** Posición Y. */
  Color_ARGB color; /** Color 32 bits. */
} Punto;

void imprime_Punto(Punto punto, FILE* salida)
{
  imprime_double(punto.x, salida); putc('\n', salida);
  imprime_double(punto.y, salida); putc('\n', salida);
  imprime_Color_ARGB(punto.color, salida); putc('\n', salida);
}

int main(void)
{
  Punto punto = { .x = 12.3, .y = 4.56, .color = 0xFFa3f193 };
  imprime_Punto(punto, stderr);
}
