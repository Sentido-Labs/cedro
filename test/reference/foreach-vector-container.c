#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h> // Para memcpy().

typedef struct str { uint8_t* start; uint8_t* end; } str;
typedef char* cstr; // Nombres de tipo deben ser palabras.


/** Tipo Vector (ristra extensible). */
typedef struct {
  float* _;
  size_t len;
  size_t capacity;
} Vec_float;

/** Añade una porción a un vector de elementos de este tipo. */
bool
añade_Vec_float(Vec_float *v, const float *start, const float *end)
{
  const size_t to_add = (size_t)(end - start);
  if (v->len + to_add > v->capacity) {
    const size_t new_capacity = v->len +
        (to_add < v->len? v->len: to_add);
    float * const new_start =
        realloc(v->_, new_capacity * sizeof(float));
    if (!new_start) return false;
    v->_        = new_start;
    v->capacity = new_capacity;
  }
  memcpy(v->_ + v->len, start, to_add * sizeof(float));
  v->len += to_add;

  return true;
}

/** Tipo Vector (ristra extensible). */
typedef struct {
  str* _;
  size_t len;
  size_t capacity;
} Vec_str;

/** Añade una porción a un vector de elementos de este tipo. */
bool
añade_Vec_str(Vec_str *v, const str *start, const str *end)
{
  const size_t to_add = (size_t)(end - start);
  if (v->len + to_add > v->capacity) {
    const size_t new_capacity = v->len +
        (to_add < v->len? v->len: to_add);
    str * const new_start =
        realloc(v->_, new_capacity * sizeof(str));
    if (!new_start) return false;
    v->_        = new_start;
    v->capacity = new_capacity;
  }
  memcpy(v->_ + v->len, start, to_add * sizeof(str));
  v->len += to_add;

  return true;
}

/** Tipo Vector (ristra extensible). */
typedef struct {
  cstr* _;
  size_t len;
  size_t capacity;
} Vec_cstr;

/** Añade una porción a un vector de elementos de este tipo. */
bool
añade_Vec_cstr(Vec_cstr *v, const cstr *start, const cstr *end)
{
  const size_t to_add = (size_t)(end - start);
  if (v->len + to_add > v->capacity) {
    const size_t new_capacity = v->len +
        (to_add < v->len? v->len: to_add);
    cstr * const new_start =
        realloc(v->_, new_capacity * sizeof(cstr));
    if (!new_start) return false;
    v->_        = new_start;
    v->capacity = new_capacity;
  }
  memcpy(v->_ + v->len, start, to_add * sizeof(cstr));
  v->len += to_add;

  return true;
}

#define añade(VEC, START, END) _Generic((VEC), \
  Vec_float*: añade_Vec_float, \
  Vec_str*: añade_Vec_str, \
  Vec_cstr*: añade_Vec_cstr \
  )(VEC, START, END)

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
