#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

int repite_letra(char letra, size_t cuenta,
                 char* nombre_de_fichero)
{
    char* texto = malloc(cuenta + 1);
    if (!texto) return ENOMEM;
    for (size_t i = 0; i < cuenta; ++i) {
        texto[i] = letra;
    }
    texto[cuenta] = 0;
    if (nombre_de_fichero) {
        FILE* fichero = fopen(nombre_de_fichero, "w");
        if (!fichero) {
            free(texto);
            return errno;
        }
        fwrite(texto, sizeof(char), cuenta, fichero);
        fputc('\n', file);
        fclose(fichero);
    }
    printf("Repetido %lu veces: %s\n",
           cuenta, texto);
    free(texto);
    return 0;
}

int main(void)
{
    return repite_letra('A', 6, "aaaaaa.txt");
}
