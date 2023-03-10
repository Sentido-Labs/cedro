// Define enum type and string form for two types.
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


static const char* const Command_STRING[] = {
  "help",
  "copy",
  "move",
  "delete"
};
static const char* const Command_SHORTOPT[] = {
  "h",
  "c",
  "m",
  "d"
};
static const char* const Command_DESCRIPTION[][2] = {
  {"Muestra este mensaje.",
  "Show this message."},
  {"Copia el fichero a la valija de almacenamiento.",
  "Copy file to storage vault."},
  {"Mueve el fichero a la valija de almacenamiento,"
  " borrando el original.",
  "Move file to storage vault, deleting the original."},
  {"Borra el fichero."
  "Delete file."}
};
enum Command {
  COMMAND_HELP = 0,
  COMMAND_COPY,
  COMMAND_MOVE,
  COMMAND_DELETE
} TIPO;
