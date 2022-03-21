char* reserva_bloque(size_t n, char** err_p)
{
    char* resultado = malloc(n);

    if (n > 10) {
        *err_p = "n es demasiado grande";
    }

    if (*err_p) {
        free(resultado);
        resultado = NULL;
    }

    return resultado;
}
