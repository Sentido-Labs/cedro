#pragma Cedro 1.0
void imprime_double(double n, FILE* salida)
{
  fprintf(salida, "%f", n);
}

typedef uint32_t Color_ARGB;
void imprime_Color_ARGB(Color_ARGB c, FILE* salida)
{
  fprintf(salida, "#%.6X", c);
}

#foreach { CAMPOS {{                            \
  { double,     x,     /** Posición X. */    }, \
  { double,     y,     /** Posición Y. */    }, \
  { Color_ARGB, color, /** Color 32 bits. */ }  \
  }}
typedef struct Punto {
#foreach { {TIPO, NOMBRE, COMENTARIO} CAMPOS
  TIPO NOMBRE; COMENTARIO
#foreach }
} Punto;

void imprime_Punto(Punto punto, FILE* salida)
{
#foreach { {TIPO, NOMBRE, COMENTARIO} CAMPOS
  imprime_##TIPO(punto.NOMBRE, salida); putc('\n', salida);
#foreach }
}
#foreach }
