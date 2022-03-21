#pragma Cedro 1.0
#foreach { VALORES {{ESPACIO, NÚMERO,                           \
                     PALABRA_CLAVE, IDENTIFICADOR, OPERADOR}}
typedef enum {
#foreach { V VALORES
  T_##V#,
#foreach }
} TipoDePieza;

const char* const TipoDePieza_CADENA[] = {
#foreach { V VALORES
  #V#,
#foreach }
};
#foreach }
