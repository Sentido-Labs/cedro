#pragma Cedro 1.0
#define { macro(A, B, C)
/// Version of f() for type A.
f_##A(B, C) /// Without semicolon “;” at the end.
#define }

int main(void) {
    int x = 1, y = 2;
    macro(int, x, y);
    // → f_int(x, y);
}
