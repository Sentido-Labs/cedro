/* -*- coding: utf-8 c-basic-offset: 2 tab-width: 2 indent-tabs-mode: nil -*- */
/** \file */
/** \mainpage
 * Cedro C pre-processor piped through GCC.
 *
 * \author Alberto González Palomo https://sentido-labs.com
 * \copyright ©2021 Alberto González Palomo https://sentido-labs.com
 *
 * Created: 2021-05-17 11:41
 */

/* _POSIX_C_SOURCE is needed for popen()/pclose(). */
#define _POSIX_C_SOURCE 200112L

#define main cedro_main
#include "./cedro.c"
#undef main

int main(int argc, char* argv[])
{
  int return_code = 0;

  Options options = {
    .discard_comments = false,
    .discard_space    = false,
    .apply_macros     = true,
    .print_markers    = false,
  };

  char* file_name = NULL;

  char** args = malloc(sizeof(char*) * (size_t)argc);
  size_t i = 0;
  args[i++] = "clang -x c -";
  for (size_t j = 1; j < argc; ++j) {
    char* arg = argv[j];
    if (!file_name && arg[0] != '-') {
      char* extension = strrchr(arg, '.');
      if (extension && 0 == strcmp(extension, ".c")) {
        file_name = arg;
        continue;
      }
    }
    if (0 == strcmp(arg, "-h") || 0 == strcmp(arg, "--help")) {
      fputs(strn_eq(getenv("LANG"), "es", 2)?
            "Uso: cedrocc [opciones] <fichero.c>\n"
            "Ejecuta cedro en el primer nombre de fichero que acabe en «.c»,\n"
            "y compila el resultado con GCC usando las opciones dadas.\n":
            "Usage: cedrocc [options] <file.c>\n"
            "Runs cedro on the first file name that ends with “.c”,\n"
            "and compiles the result with GCC using the given options.\n",
            stderr);
      free(args);
      return 0;
    }
    args[i++] = arg;
  }
  assert(i <= argc);

  if (! file_name) {
    fprintf(stderr, "Missing file name.");
    free(args);
    return 1;
  }

  size_t length = 0;
  for (size_t j = 0; j < i; ++j) {
    length += (j == 0? 0: 1) + strlen(args[j]);
  }

  ++length; // Make space for the zero terminator.
  char* cmd = malloc(length);
  cmd[0] = 0;
  // Quadratic performance, but the string is small anyway.
  // Can be optimized by keeping track of the offset for the next write.
  for (size_t j = 0; j < i; ++j) {
    if (j != 0) strncat(cmd, " ", length);
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
  while (macro->name && macro->function) {
    macro->function(&markers, &src);
    ++macro;
  }

  fflush(stderr);
  fflush(stdout);

  FILE* gcc_stdin = popen(cmd, "w");
  unparse(&markers, &src, options, gcc_stdin);
  return_code = pclose(gcc_stdin);

  fflush(stdout);

  destruct_Marker_array(&markers);
  destruct_Byte_array(&src);

  return return_code;
}
