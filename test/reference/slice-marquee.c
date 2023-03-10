#ifdef _WIN32
#include <windows.h>
// Véase https://stackoverflow.com/a/3930716/
int sleep_ms(int ms) { Sleep(ms); return 0; }
#else
#define _XOPEN_SOURCE 500
#include <unistd.h> // Desfasado pero simple:
int sleep_ms(int ms) { return usleep(ms*1000); }
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef char utf8_char[5]; // Como cadena C.
void
imprime_porción(const utf8_char* inicio,
                const utf8_char* final)
{
  while (inicio < final) fputs(*inicio++, stdout);
}

int
main(int argc, char* argv[])
{
  int tamaño_letrero = 8;
  if (argc < 2) {
    fprintf(stderr, "Uso: letrero <texto>\n",
            tamaño_letrero);
    exit(1);
  }

  const char separador[] = " *** ";
  size_t lon_octetos   = strlen(argv[1]);
  size_t lon_separador = strlen(separador);
  char* m = malloc(lon_octetos + lon_separador);
  memcpy(m,               argv[1],   lon_octetos);
  memcpy(m + lon_octetos, separador, lon_separador);
  lon_octetos += lon_separador;

  // Extrae cada carácter codificado como UTF-8,
  // que necesita hasta 4 octetos + 1 terminador.
  utf8_char* mensaje =
    malloc(sizeof(utf8_char) * lon_octetos);
  size_t lon = 0;
  for (size_t final = 0; final < lon_octetos;) {
    const char b = m[final];
    size_t u;
    if      (0xF0 == (b & 0xF8)) u = 4;
    else if (0xE0 == (b & 0xF0)) u = 3;
    else if (0xC0 == (b & 0xE0)) u = 2;
    else                         u = 1;
    if (final + u > lon_octetos) break;
    memcpy(&mensaje[lon], &m[final], u);
    mensaje[final][u] = '\0';
    final += u;
    ++lon;
  }

  if (lon < 2) {
    fprintf(stderr, "texto de mensaje demasiado corto.\n");
    exit(2);
  } else if (lon < tamaño_letrero) {
    tamaño_letrero = lon - 1;
  }

  for (;;) {
    for (int i = 0; i < lon; ++i) {
      int resto = i + tamaño_letrero > lon?
          i + tamaño_letrero - lon: 0;
      int visible = tamaño_letrero - resto;
      imprime_porción(&mensaje[i], &mensaje[i+visible]);
      imprime_porción(&mensaje[0], &mensaje[resto]);
      putc('\r', stdout);
      fflush(stdout);
      sleep_ms(300);
    }
  }

  free(mensaje);
  free(m);

  return 0;
}
