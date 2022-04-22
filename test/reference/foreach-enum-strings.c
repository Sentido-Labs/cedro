typedef enum {
  T_ESPACIO,
  T_NÚMERO,
  T_PALABRA_CLAVE,
  T_IDENTIFICADOR,
  T_OPERADOR
} TipoDePieza;

const char* const TipoDePieza_CADENA[] = {
  "ESPACIO",
  "NÚMERO",
  "PALABRA_CLAVE",
  "IDENTIFICADOR",
  "OPERADOR"
};

typedef enum {
  M_ENTRADA,
  M_SALIDA
} ConfDePúa;

const char* const ConfDePúa_CADENA[] = {
  "ENTRADA",
  "SALIDA"
};
