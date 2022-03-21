#define macro(A, B, C)                                                        \
f_##A(B, C); /** Versión de f() para el tipo A. */                            \
/* End #define */
