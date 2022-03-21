#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h> // Para memcpy().

typedef struct str { uint8_t* start; uint8_t* end; } str;
typedef char* cstr; // Nombres de tipo deben ser palabras.

#pragma Cedro 1.0

#foreach { LISTA_DE_TIPOS {{float, str, cstr}}
#foreach { T LISTA_DE_TIPOS
/** Tipo Vector (ristra extensible). */
typedef struct {
  T* _;
  size_t len;
  size_t capacity;
} Vec_##T;

/** Añade una porción a un vector de elementos de este tipo. */
bool
añade_Vec_##T(Vec_##T *v, const T *start, const T *end)
{
  const size_t to_add = (size_t)(end - start);
  if (v->len + to_add > v->capacity) {
    const size_t new_capacity = v->len +
        (to_add < v->len? v->len: to_add);
    T * const new_start =
        realloc(v->_, new_capacity * sizeof(T));
    if (!new_start) return false;
    v->_        = new_start;
    v->capacity = new_capacity;
  }
  memcpy(v->_ + v->len, start, to_add * sizeof(T));
  v->len += to_add;

  return true;
}

#foreach }
#foreach { DEFINE {#define} // Mantiene separadas las líneas.
DEFINE añade(VEC, START, END) _Generic((VEC), \
#foreach { T LISTA_DE_TIPOS
  Vec_##T*: añade_Vec_##T#, \
#foreach }
  )(VEC, START, END)
#foreach }
#foreach }

#include <stdio.h>
int main(void)
{
  Vec_cstr palabras = {0};
  cstr animales[] = { "caballo", "gato", "pollo", "perro" };
  cstr plantas [] = { "rábano", "trigo", "tomate" };
  añade(&palabras, &animales[0], &animales[2]);
  añade(&palabras, &plantas[0], &plantas[3]);
  añade(&palabras, &animales[2], &animales[4]);
  for (cstr *p = palabras._, *fin = palabras._ + palabras.len;
       p != fin; ++p) {
    fprintf(stderr, "Palabra: \"%s\"\n", *p);
  }
  return 0;
}
