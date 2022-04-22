#pragma Cedro 1.0
#foreach { {TIPO, PREFIJO, VALORES} {                        \
  { TipoDePieza, T_, {ESPACIO, NÚMERO,                       \
                  PALABRA_CLAVE, IDENTIFICADOR, OPERADOR} }, \
  { ConfDePúa,   M_, {ENTRADA, SALIDA} }                     \
  }
typedef enum {
#foreach { V VALORES
  PREFIJO##V#,
#foreach }
} TIPO;

const char* const TIPO##_CADENA[] = {
#foreach { V VALORES
  #V#,
#foreach }
};

#foreach }
