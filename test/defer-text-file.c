#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#pragma Cedro 1.0

int repite_letra(char letra, size_t cuenta,
                 char* nombre_de_fichero)
{
    char* texto = malloc(cuenta + 1);
    if (!texto) return ENOMEM;
    auto free(texto);
    for (size_t i = 0; i < cuenta; ++i) {
        texto[i] = letra;
    }
    texto[cuenta] = 0;
    if (nombre_de_fichero) {
        FILE* fichero = fopen(nombre_de_fichero, "w");
        if (!fichero) return errno;
        auto fclose(fichero);
        fwrite(texto, sizeof(char), cuenta, fichero);
        fputc('\n', file);
    }
    printf("Repetido %lu veces: %s\n",
           cuenta, texto);
    return 0;
}

int main(void)
{
    return repite_letra('A', 6, "aaaaaa.txt");
}
