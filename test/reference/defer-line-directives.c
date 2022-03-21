#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
int repite_letra(char letra, size_t cuenta,
#line 8 "test/defer-line-directives.c"
                 char* nombre_de_fichero)
{
    char* texto = malloc(cuenta + 1);
    if (!texto) return ENOMEM;
#line 13 "test/defer-line-directives.c"
    for (size_t i = 0; i < cuenta; ++i) {
        texto[i] = letra;
    }
    texto[cuenta] = 0;
    if (nombre_de_fichero) {
        FILE* fichero = fopen(nombre_de_fichero, "w");
        if (!fichero) {
#line 12 "test/defer-line-directives.c"
            free(texto);
#line 19 "test/defer-line-directives.c"
            return errno;
#line 20 "test/defer-line-directives.c"
        }
#line 21 "test/defer-line-directives.c"
        fwrite(texto, sizeof(char), cuenta, fichero);
        fputc('\n', file);
#line 20 "test/defer-line-directives.c"
        fclose(fichero);
#line 23 "test/defer-line-directives.c"
    }
    printf("Repetido %lu veces: %s\n",
           cuenta, texto);
#line 12 "test/defer-line-directives.c"
    free(texto);
#line 26 "test/defer-line-directives.c"
    return 0;
}

int main(void)
{
    return repite_letra('A', 6, "aaaaaa.txt");
}
