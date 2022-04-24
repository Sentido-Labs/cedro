#define macro(A, B, C)                                                        \
/** Version of f() for type A. */                                             \
f_##A(B, C) /** Semicolon “;” removed by Cedro. */                            \
/* End #define */

int main(void) {
    int x = 1, y = 2;
    macro(int, x, y);
    // → f_int(x, y);
}


/** Expand to the mutable and constant variants for a `typedef`. \n
 * The default, just the type name `T`, is the constant variant
 * and the mutable variant is named `mut_T`, with corresponding \n
 * `T_p`: constant pointer to constant `T`       \n
 * `mut_T_p`: constant pointer to mutable `T`    \n
 * `mut_T_mut_p`: mutable pointer to mutable `T` \n
 * This mimics the usage in Rust, where constant bindings are the default
 * which is a good idea.
 */
#define MUT_CONST_TYPE_VARIANTS(T)                                            \
/*  */      mut_##T,                                                          \
    *       mut_##T##_mut_p,                                                  \
    * const mut_##T##_p;                                                      \
typedef const mut_##T T,                                                      \
    *                 T##_mut_p,                                              \
    * const           T##_p                                                   \
/* End #define */

typedef enum Error {
    ERROR_NONE, ERROR_INIT_FAIL_GLFW, ERROR_INIT_FAIL_GLEW, ERROR_INIT_FAIL_NANOVG, ERROR_OPEN_FAIL_WINDOW
} MUT_CONST_TYPE_VARIANTS(Error);
