#pragma Cedro 1.0
char* reserva_bloque(size_t n, char** err_p)
{
    char* resultado = malloc(n);
    auto if (*err_p) {
        free(resultado);
        resultado = NULL;
    }

    if (n > 10) {
        *err_p = "n es demasiado grande";
    }

    return resultado;
}
