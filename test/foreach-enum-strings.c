#pragma Cedro 1.0
// Define enum type and string form for two types.
#foreach { {TIPO, PREFIJO, VALORES} {                        \
  { TipoDePieza, T_, {ESPACIO,   NÚMERO,                     \
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

#foreach { COMMANDS {{                                      \
    {HELP = 0, help  , h,                                   \
    {"Muestra este mensaje.",                               \
    "Show this message."}},                                 \
    {COPY,     copy  , c,                                   \
    {"Copia el fichero a la valija de almacenamiento.",     \
    "Copy file to storage vault."}},                        \
    {MOVE,     move  , m,                                   \
    {"Mueve el fichero a la valija de almacenamiento,"      \
    " borrando el original.",                               \
    "Move file to storage vault, deleting the original."}}, \
    {DELETE,   delete, d,                                   \
    {"Borra el fichero."                                    \
    "Delete file."}}                                        \
}}
static const char* const Command_STRING[] = {
#foreach { {CMD,STR,OPT,DESC} COMMANDS
  #STR#,
#foreach }
};
static const char* const Command_SHORTOPT[] = {
#foreach { {CMD,STR,OPT,DESC} COMMANDS
  #OPT#,
#foreach }
};
static const char* const Command_DESCRIPTION[][2] = {
#foreach { {CMD,STR,OPT,DESC} COMMANDS
  DESC#,
#foreach }
};
enum Command {
#foreach { {CMD,STR,OPT,DESC} COMMANDS
  COMMAND_##CMD#,
#foreach }
} TIPO;
#foreach }
