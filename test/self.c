#include <stdio.h>
#include <stdlib.h>

#pragma Cedro 1.0 self

typedef struct Number *Number_p;
typedef double (*Number_as_double)(Number_p n);
typedef double (*Number_delete   )(Number_p n);
typedef struct Number {
  Number_delete    delete;
  Number_as_double as_double;
} Number;

typedef struct Integer {
  Number_as_double delete;
  Number_as_double as_double;
  int a;
} Integer, *Integer_p;
double
as_double_Integer(Integer_p n)
{
  return (double)(n->a);
}
void
delete_Integer(Integer_p self)
{
  free(self);
}
void*
new_Integer(int a)
{
  Integer_p n = malloc(sizeof(Integer));
  n->as_double = (Number_as_double)as_double_Integer;
  n->delete    = (Number_delete   )   delete_Integer;
  n->a = a;
  return n;
}

typedef struct Fraction {
  Number_as_double delete;
  Number_as_double as_double;
  Integer_p a; Integer_p b;
} Fraction, *Fraction_p;
void
delete_Fraction(Fraction_p self)
{
  self->a->delete();
  self->b->delete();
  free(self);
}
double
as_double_Fraction(Fraction_p n)
{
  return n->a->as_double() / n->b->as_double();
}
void*
new_Fraction(int a, int b)
{
  Fraction_p n = malloc(sizeof(Fraction));
  n->as_double = (Number_as_double)as_double_Fraction;
  n->delete    = (Number_delete   )   delete_Fraction;
  n->a = new_Integer(a);
  n->b = new_Integer(b);
  return n;
}

int
main(int argc, char* argv[])
{
  Number_p numbers[] = {
    new_Fraction(3, 7),
    new_Integer(8)
  };
  size_t numbers_len = sizeof(numbers) / sizeof(Number_p);
  double result = 1;
  for (size_t i = 0; i < numbers_len; ++i) {
    printf("%d: %f\n", i, numbers[i]->as_double());
    result *= numbers[i]->as_double();
  }
  printf("Product of all numbers: %f\n", result);
  for (size_t i = 0; i < numbers_len; ++i) numbers[i]->delete();
}
