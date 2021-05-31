/* -*- coding: utf-8 c-basic-offset: 2 tab-width: 2 indent-tabs-mode: nil -*- */
/** \file */
/** \mainpage
 * Cedro C pre-processor piped through the system’s C compiler, cc.
 *
 * \author Alberto González Palomo https://sentido-labs.com
 * \copyright ©2021 Alberto González Palomo https://sentido-labs.com
 *
 * Created: 2021-05-17 11:41
 */

/* _POSIX_C_SOURCE is needed for popen()/pclose(). */
#define _POSIX_C_SOURCE 200112L

#define USE_CEDRO_AS_LIBRARY
#include "./cedro.c"

static const char* const
usage_es =
    "Uso: cedrocc [opciones] <fichero.c> [ fichero2.o … ]\n"
    "Ejecuta Cedro en el primer nombre de fichero que acabe en «.c»,\n"
    "y compila el resultado con «%s» mas los otros argumentos.\n"
    "Para usar otro compilador, p.ej. gcc: CEDRO_CC='gcc -x c -' cedrocc …\n"
    ;
static const char* const
usage_en =
    "Usage: cedrocc [options] <file.c> [ file2.o … ]\n"
    "Runs Cedro on the first file name that ends with “.c”,\n"
    "and compiles the result with “%s” plus the other arguments.\n"
    "To use another compiler, e.g. gcc: CEDRO_CC='gcc -x c -' cedrocc …\n"
    ;

int main(int argc, char* argv[])
{
  int return_code = 0;

  char* cc = getenv("CEDRO_CC");
  if (cc) {
    eprintln(LANG("Usando CEDRO_CC='%s'\n", "Using CEDRO_CC='%s'\n"), cc);
  } else {
    cc = "cc -x c -";
  }

  Options options = {
    .apply_macros     = true,
    .escape_ucn       = false,
    .discard_comments = false,
    .discard_space    = false,
  };

  char* file_name = NULL;

  // The number of arguments is either the same if no .c file name given,
  // or one less when extracting the .c file name.
  char** args = malloc(sizeof(char*) * (size_t)argc);
  size_t i = 0;
  args[i++] = cc;
  for (size_t j = 1; j < argc; ++j) {
    char* arg = argv[j];
    if (not file_name and arg[0] is_not '-') {
      char* extension = strrchr(arg, '.');
      if (extension and str_eq(extension, ".c")) {
        file_name = arg;
        continue;
      }
    }
    if (str_eq(arg, "-h") or str_eq(arg, "--help")) {
      eprintln(LANG(usage_es, usage_en), args[0]);
      free(args);
      return 0;
    }
    args[i++] = arg;
  }
  assert(i <= argc);

  if (not file_name) {
    eprintln("Missing file name.");
    free(args);
    return 1;
  }

  size_t length = 0;
  for (size_t j = 0; j < i; ++j) {
    length += (j is 0? 0: 1) + strlen(args[j]);
  }

  ++length; // Make space for the zero terminator.
  char* cmd = malloc(length);
  cmd[0] = 0;
  // Quadratic performance, but the string is small anyway.
  // Can be optimized by keeping track of the offset for the next write.
  for (size_t j = 0; j < i; ++j) {
    if (j is_not 0) strncat(cmd, " ", length);
    strncat(cmd, args[j], length);
  }

  free(args);

  mut_Marker_array markers;
  init_Marker_array(&markers, 8192);

  mut_Byte_array src;
  read_file(&src, file_name);

  markers.len = 0;

  parse(&src, &markers);

  Macro_p macro = macros;
  while (macro->name and macro->function) {
    macro->function(&markers, &src);
    ++macro;
  }

  fflush(stderr);
  fflush(stdout);

  FILE* cc_stdin = popen(cmd, "w");
  unparse(&markers, &src, options, cc_stdin);
  return_code = pclose(cc_stdin);

  fflush(stdout);

  destruct_Marker_array(&markers);
  destruct_Byte_array(&src);

  return return_code;
}
