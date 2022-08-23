/* -*- coding: utf-8 c-basic-offset: 2 tab-width: 2 indent-tabs-mode: nil -*-
 * vi: set et ts=2 sw=2: */
/** \file */
/** \mainpage
 * Cedro C pre-processor piped through the system’s C compiler, cc.
 *
 * \author Alberto González Palomo https://sentido-labs.com
 * \copyright ©2021 Alberto González Palomo https://sentido-labs.com
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Created: 2021-05-17 11:41
 */

#pragma Cedro 1.0

/* _POSIX_C_SOURCE is needed for popen()/pclose(). */
#define _POSIX_C_SOURCE 200112L
/* In Solaris 8, we need __EXTENSIONS__ for popen()/pclose() and vsnprintf(). */
#define __EXTENSIONS__

#define USE_CEDRO_AS_LIBRARY
#include "cedro.c"

#include <unistd.h>
#include <sys/wait.h> // For WEXITSTATUS etc.

typedef size_t mut_size_t, * mut_size_t_mut_p, * const mut_size_t_p;
typedef const size_t * size_t_mut_p, * const size_t_p;
DEFINE_ARRAY_OF(size_t, 0, {});
typedef struct IncludePaths {
  mut_Byte_array text;
  mut_size_t_array lengths;
} MUT_CONST_TYPE_VARIANTS(IncludePaths);
mut_IncludePaths
init_IncludePaths(size_t initial_capacity)
{
  return (mut_IncludePaths){
    .text    = init_Byte_array(initial_capacity * 40),
    .lengths = init_size_t_array(initial_capacity)
  };
}
static void
destruct_IncludePaths(mut_IncludePaths_p _)
{
  destruct_Byte_array(&_->text);
  destruct_size_t_array(&_->lengths);
}
static size_t
len_IncludePaths(IncludePaths_p _) { return _->lengths.len; }
static void
truncate_IncludePaths(mut_IncludePaths_p _, size_t len)
{
  if (len >= _->lengths.len) return;

  size_t text_len = 0;
  for (size_t i = 0; i is_not len; ++i) {
    text_len += *get_size_t_array(&_->lengths, i);
  }
  _->text.len = text_len;
  _->lengths.len = len;
}
static Byte_array_slice
get_IncludePaths(IncludePaths_p _, size_t index)
{
  Byte_mut_p start = start_of_Byte_array(&_->text);
  for (size_t i = 0; i is_not index; ++i) {
    start += *get_size_t_array(&_->lengths, i);
  }
  return (Byte_array_slice){
    start,
    start + *get_size_t_array(&_->lengths, index)
  };
}
static void
append_path(mut_IncludePaths_p _, const char* path, size_t path_len)
{
  size_t position = 0;
  Byte_mut_p start = start_of_Byte_array(&_->text);
  while (position is_not _->lengths.len) {
    size_t len = *get_size_t_array(&_->lengths, position);
    if (len is path_len and mem_eq(start, B(path), len)) return; // Duplicate.
    start += len;
    ++position;
  }
  append_Byte_array(&_->text,
                    (Byte_array_slice){ B(path), B(path) + path_len });
  push_size_t_array(&_->lengths, path_len);
}
static void
prepend_path(mut_IncludePaths_p _, const char* path, size_t len)
{
  splice_Byte_array(&_->text, 0, 0, NULL,
                    (Byte_array_slice){ (Byte_p)path[0..len] });
  splice_size_t_array(&_->lengths, 0, 0, NULL,
                      (size_t_array_slice){ &len, (&len) + 1 });
}
#define { DEFINE_RESULT_TYPE_FOR(T)
typedef struct Result_##T { int error; T value; } Result_##T
#define }
DEFINE_RESULT_TYPE_FOR(size_t);
static Result_size_t
find_path_in(IncludePaths_p _, const char* file_name)
{
  size_t position = 0;

  Byte_mut_p start = start_of_Byte_array(&_->text);
  while (position is_not _->lengths.len) {
    size_t len = *get_size_t_array(&_->lengths, position);
    if (mem_eq(start, B(file_name), len)) return (Result_size_t){ 0, position };
    start += len;
    ++position;
  }

  return (Result_size_t){ 1, position };
}

typedef struct SourceFile {
  mut_Byte_array path;
  mut_Byte_array src;
  mut_Marker_array markers;
} MUT_CONST_TYPE_VARIANTS(SourceFile);
DEFINE_ARRAY_OF(SourceFile, 0, {
    while (cursor is_not end) {
      destruct_Marker_array(&cursor->markers);
      destruct_Byte_array(&cursor->src);
      destruct_Byte_array(&cursor->path);
      ++cursor;
    }
  });
static SourceFile_p
get_SourceFile_for_path(mut_SourceFile_array_p _, Byte_array path)
{
  SourceFile_mut_p cursor = start_of_SourceFile_array(_);
  SourceFile_p        end =   end_of_SourceFile_array(_);
  while (cursor is_not end) {
    if (cursor->path.len is path.len and
        mem_eq(start_of_Byte_array(&cursor->path),
               start_of_Byte_array(&path), path.len)) {
      return cursor;
    }
    ++cursor;
  }

  return NULL;
}
/** Find a file in the given paths.
 * `result` is modified only if the file is found.
 */
static bool
find_include_file(IncludePaths_p _, Byte_array_slice path,
                  mut_Byte_array_p result)
{
  bool found = false;

  mut_Byte_array path_buffer = init_Byte_array(256);
  size_t i = len_IncludePaths(_);
  while (i is_not 0) {
    --i;
    path_buffer.len = 0;
    append_Byte_array(&path_buffer, get_IncludePaths(_, i));
    push_Byte_array(&path_buffer, '/');
    append_Byte_array(&path_buffer, path);
    if (access(as_c_string(&path_buffer), F_OK) is 0) {
      result->len = 0;
      append_Byte_array(result, bounds_of_Byte_array(&path_buffer));
      found = true;
      break;
    }
  }
  destruct_Byte_array(&path_buffer);

  return found;
}

typedef struct IncludeContext {
  size_t level;
  mut_IncludePaths paths;
  mut_IncludePaths paths_quote;
} mut_IncludeContext, *mut_IncludeContext_p;
typedef const struct IncludeContext IncludeContext,
  * const IncludeContext_p, * IncludeContext_mut_p;

static int
include(const char* file_name, FILE* cc_stdin,
        mut_IncludeContext_p context,
        Options options);

/**
 * @param[in] m marker for the `#include` line.
 */
static int
include_callback(Marker_p m, Byte_array_p src, FILE* cc_stdin,
                 void* context, Options options)
{
  mut_IncludeContext_p _ = context;

  int return_code = EXIT_SUCCESS;

  Byte_array_mut_slice content = slice_for_marker(src, m);
  Byte_mut_p text = content.start_p;
  bool quoted_include = false;
  const size_t len = 10; // = strlen("#include <");
  if        (m->len >= len and strn_eq("#include <",  (char*)text, len)) {
    content.start_p += len;
    // TODO: check whether the C standard allows escaped '>' here.
    content.end_p = memchr(text + len, '>', m->len - len);
  } else if (m->len >= len and strn_eq("#include \"", (char*)text, len)) {
    quoted_include = true;
    content.start_p += len;
    // TODO: check whether the C standard allows escaped '"' here.
    content.end_p = memchr(text + len, '"', m->len - len);
  }

  if (content.end_p > content.start_p) {
    ++_->level;
    mut_Byte_array s = {0};
    auto destruct_Byte_array(&s);
    if ((quoted_include and
         find_include_file(&_->paths_quote, content, &s)) or
        find_include_file(&_->paths, content, &s)) {
      // TODO: check #define guards?
      size_t previous_len = len_IncludePaths(&_->paths_quote);
      const char* file_dir_name_end = strrchr(as_c_string(&s), '/');
      if (file_dir_name_end) {
        append_path(&_->paths_quote, as_c_string(&s),
                    (size_t)(file_dir_name_end - as_c_string(&s)));
      }
      return_code = include(as_c_string(&s), cc_stdin, _, options);
      truncate_IncludePaths(&_->paths_quote, previous_len);
      if (return_code is EXIT_SUCCESS) {
        fprintf(cc_stdin, "\n#line %lu \"",
                original_line_number(m->start, src));
        fwrite(content.start_p, sizeof(content.start_p[0]),
               (size_t)(content.end_p - content.start_p), cc_stdin);
        fprintf(cc_stdin, "\"\n");
      }
    } else {
      // The file was not found, assume it’s not a Cedro file.
      return_code = -1;
    }
    --_->level;
  } else {
    fprintf(cc_stdin, LANG("\n#error #include vacío.\n",
                           "\n#error empty #include.\n"));
  }

  return return_code;
}

/**
   Returns either `EXIT_SUCCESS` (that is, `0`),
   an error code as defined in errno.h
   (“Values for errno are now required to be distinct positive values”),
   or `-1` if there was no error but the included file does not contain
   the Cedro `#pragma` so it does not need to be expanded in place.
 */
static int
include(const char* file_name, FILE* cc_stdin,
        mut_IncludeContext_p context,
        mut_Options options)
{
  if (context->level > 10) {
    eprintln(LANG("Error: demasiada recursión de «include» en: %s",
                  "Error: too much “include” recursion at: %s"),
             file_name);
    return EINVAL;
  }

  int return_code = EXIT_SUCCESS;

  mut_Marker_array markers = init_Marker_array(8192);
  auto destruct_Marker_array(&markers);

  mut_Byte_array src = {0};
  auto destruct_Byte_array(&src);
  int err = read_file(&src, file_name);
  if (err) {
    print_file_error(err, file_name, src.len);
    return err;
  } else {
    Byte_array_mut_slice region = bounds_of_Byte_array(&src);
    region.start_p = parse_skip_until_cedro_pragma(&src, region, &markers,
                                                   &options);
    Byte_p parse_end = parse(&src, region, &markers, false);
    if (parse_end is_not region.end_p) {
      if (fprintf(cc_stdin, "#line %lu \"%s\"\n#error %s\n",
                  original_line_number((size_t)(parse_end - src.start), &src),
                  file_name,
                  error_buffer) < 0) {
        eprintln(LANG("error al escribir la directiva #line",
                      "error when writing #line directive"));
      }
      error_buffer[0] = 0;
    }

    if (options.enable_embed_directive and options.embed_as_string) {
      err = prepare_binary_embedding(&markers, &src, file_name);
      if (err) {
        eprintln("#line %lu \"%s\"\n#error %s\n",
                 original_line_number((size_t)(parse_end - src.start), &src),
                 file_name,
                 error_buffer);
        return 73;
      }
    }

    if (context->level is_not 0) {
      if (markers.len is 1) {
        // Included file is not a Cedro file.
        return -1;
      } else {
        // Does not depend on options.insert_line_directives
        // because it does not cause syntax problems
        // as it is right at the beginning of an input file.
        fprintf(cc_stdin, "\n#line %lu \"%s\"\n",
                original_line_number((size_t)(region.start_p - src.start),
                                     &src),
            file_name);
      }
    }

    size_t original_src_len = src.len;

    Macro_p macro = macros;
    while (macro->name and macro->function) {
      macro->function(&markers, &src);
      ++macro;
    }

    fflush(stderr);
    fflush(stdout);

    IncludeCallback include = {
      &include_callback,
      context
    };
    mut_Replacement_array replacements = {0};
    unparse_fragment(markers.start, end_of_Marker_array(&markers), 0,
                     &src, original_src_len,
                     file_name, &include,
                     &replacements, false,
                     options, cc_stdin);
    destruct_Replacement_array(&replacements);

    if (error_buffer[0]) {
      fprintf(cc_stdin, "\n#error %s\n", error_buffer);
      error_buffer[0] = 0;
    }
  }

  return return_code;
}

static const char* const
usage_es =
    "Uso: cedrocc [opciones] <fichero.c> [<fichero2.o>…]\n"
    "  Ejecuta Cedro en el primer nombre de fichero que acabe en «.c»,\n"
    " y compila el resultado con «%s» mas los otros argumentos.\n"
    "    cedrocc -o fichero fichero.c\n"
    "    cedro fichero.c | cc -x c - -o fichero\n"
    "  Las opciones se pasan tal cual al compilador, excepto las que\n"
    " empiecen con «--cedro:…» que corresponden a opciones de cedro,\n"
    " por ejemplo «--cedro:escape-ucn» es como «cedro --escape-ucn».\n"
    "  La siguiente opción es implícita:\n"
    "    --cedro:insert-line-directives\n"
    "  Algunas opciones de GCC activan opciones de Cedro automáticamente:\n"
    "    --std=c89|c90|iso9899:1990|iso8999:199409 → --cedro:c89\n"
    "    --std=c99|c9x|iso9899:1999|iso9899:199x   → --cedro:c99\n"
    "    --std=c11|c1x|iso9899:2011                → --cedro:c11\n"
    "    --std=c17|c18|iso9899:2017|iso9899:2018   → --cedro:c17\n"
    "    --std=c2x                                 → --cedro:c23\n"
    "\n"
    "  Además, para cada `#include`, si encuentra el fichero lo lee y\n"
    " si encuentra `#pragma Cedro 1.0` lo procesa e inserta el resultado\n"
    " en lugar del `#include`.\n"
    "\n"
    "  Se puede especificar el compilador, p.ej. `gcc`:\n"
    "    CEDRO_CC='gcc -x c - -x none' cedrocc …\n"
    "  Para depuración, esto escribe el código que iría entubado a `cc`,\n"
    " en `stdout`:\n"
    "    CEDRO_CC='' cedrocc …"
    ;
static const char* const
usage_en =
    "Usage: cedrocc [options] <file.c> [<file2.o>…]\n"
    "  Runs Cedro on the first file name that ends with “.c”,\n"
    " and compiles the result with “%s” plus the other arguments.\n"
    "    cedrocc -o file file.c\n"
    "    cedro file.c | cc -x c - -o file\n"
    "  The options get passed as is to the compiler, except for those that\n"
    " start with “--cedro:…” that correspond to cedro options,\n"
    " for instance “--cedro:escape-ucn” is like “cedro --escape-ucn”.\n"
    "  The following option is the default:\n"
    "    --cedro:insert-line-directives\n"
    "  Some GCC options activate Cedro options automatically:\n"
    "    --std=c89|c90|iso9899:1990|iso8999:199409 → --cedro:c89\n"
    "    --std=c99|c9x|iso9899:1999|iso9899:199x   → --cedro:c99\n"
    "    --std=c11|c1x|iso9899:2011                → --cedro:c11\n"
    "    --std=c17|c18|iso9899:2017|iso9899:2018   → --cedro:c17\n"
    "    --std=c2x                                 → --cedro:c23\n"
    "\n"
    "  In addition, for each `#include`, if it finds the file it reads it and\n"
    " if it finds `#pragma Cedro 1.0` processes it and inserts the result\n"
    " in place of the `#include`.\n"
    "\n"
    "  You can specify the compiler, e.g. `gcc`:\n"
    "    CEDRO_CC='gcc -x c - -x none' cedrocc …\n"
    "  For debugging, this writes the code that would be piped into `cc`,\n"
    " into `stdout` instead:\n"
    "    CEDRO_CC='' cedrocc …"
    ;

int main(int argc, char* argv[])
{
  mut_Options options = DEFAULT_OPTIONS;
  options.insert_line_directives = true;

  int return_code = EXIT_SUCCESS;

  char* cc = getenv("CEDRO_CC");
  if (cc) {
    eprintln(LANG("Usando CEDRO_CC='%s'\n", "Using CEDRO_CC='%s'\n"), cc);
  } else {
    cc = "cc -x c - -x none";
  }

  char* file_name = NULL;

  mut_IncludeContext include_context = (mut_IncludeContext){
    .paths       = init_IncludePaths(10),
    .paths_quote = init_IncludePaths(10)
  };
  auto destruct_IncludePaths(&include_context.paths);
  auto destruct_IncludePaths(&include_context.paths_quote);

  // The number of arguments is either the same if no .c file name given,
  // or one less when extracting the .c file name.
  char** args = malloc(sizeof(char*) * (size_t)argc);
  auto free(args);
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
    if (strn_eq(arg, "--cedro:", 8/*strlen("--cedro:")*/)) {
      /* Derived from cedro.c/main(): */
      bool flag_value = !strn_eq("--cedro:no-", arg, 11);
      if        (str_eq("--cedro:apply-macros", arg) or
                 str_eq("--cedro:no-apply-macros", arg)) {
        options.apply_macros = flag_value;
      } else if (str_eq("--cedro:escape-ucn", arg) or
                 str_eq("--cedro:no-escape-ucn", arg)) {
        options.escape_ucn = flag_value;
      } else if (str_eq("--cedro:discard-comments", arg) or
                 str_eq("--cedro:no-discard-comments", arg)) {
        options.discard_comments = flag_value;
      } else if (str_eq("--cedro:discard-space", arg) or
                 str_eq("--cedro:no-discard-space", arg)) {
        options.discard_space = flag_value;
      } else if (str_eq("--cedro:insert-line-directives", arg) or
                 str_eq("--cedro:no-insert-line-directives", arg)) {
        options.insert_line_directives = flag_value;
      } else if (str_eq("--cedro:c89", arg) or
                 str_eq("-std=c89", arg) or
                 str_eq("-std=c90", arg) or
                 str_eq("-std=iso9899:1990", arg) or
                 str_eq("-std=iso9899:199409", arg)) {
        options.c_standard = C89;
      } else if (str_eq("--cedro:c99", arg) or
                 str_eq("-std=c99", arg) or
                 str_eq("-std=c9x", arg) or
                 str_eq("-std=iso9899:1999", arg) or
                 str_eq("-std=iso9899:199x", arg)) {
        options.c_standard = C99;
      } else if (str_eq("--cedro:c11", arg) or
                 str_eq("-std=c11", arg) or
                 str_eq("-std=c1x", arg) or
                 str_eq("-std=iso9899:2011", arg)) {
        options.c_standard = C11;
      } else if (str_eq("--cedro:c17", arg) or
                 str_eq("-std=c17", arg) or
                 str_eq("-std=c18", arg) or
                 str_eq("-std=iso9899:2017", arg) or
                 str_eq("-std=iso9899:2018", arg)) {
        options.c_standard = C17;
      } else if (str_eq("--cedro:c23", arg) or
                 str_eq("-std=c2x", arg)) {
        options.c_standard = C23;
      } else if (str_eq("--cedro:embed-directive", arg) or
                 str_eq("--cedro:no-embed-directive", arg)) {
        eprintln(LANG("Aviso: la opción «%s» está obsoleta,\n"
                      "       use %s",
                      "Warning: the option “%s” is obsolete,\n"
                      "         use %s"),
                 arg,
                 flag_value?
                 LANG("--cedro:c99 (implícita) para procesar #embed.",
                      "--cedro:c99 (default) to process #embed."):
                 LANG("--cedro:c23 para dejar los #embed al compilador.",
                      "--cedro:c23 to leave the #embed to the compiler."));
        options.c_standard = flag_value? C99: C23;
      } else if (strn_eq("--cedro:embed-as-string=", arg,
                         strlen("--cedro:embed-as-string="))) {
        char* end = arg + strlen("--cedro:embed-as-string=");
        long value = strtol(end, &end, 10);
        if (errno or end is_not arg + strlen(arg)) {
          eprintln("#error Value must be an integer: %s\n", arg);
          return 12;
        } else {
          options.embed_as_string = (size_t)value;
        }
      } else if (str_eq("--cedro:defer-instead-of-auto", arg) or
                 str_eq("--cedro:no-defer-instead-of-auto", arg)) {
        eprintln(LANG("Error: la opción «%s» está obsoleta,\n"
                      "       use `#pragma Cedro 1.0 defer`.",
                      "Error: the option “%s” is obsolete,\n"
                      "       use `#pragma Cedro 1.0 defer`."),
                 arg);
        return 13;
      } else if (str_eq("--cedro:version", arg)) {
        eprintln(CEDRO_VERSION);
        return EXIT_SUCCESS;
      } else {
        eprintln(LANG("Error: opción desconocida: %s",
                      "Error: unknown option: %s"),
                 arg);
        eprintln(LANG(usage_es, usage_en), args[0]);
        return EINVAL;
      }
      continue;
    } else if (str_eq(arg, "-h") or str_eq(arg, "--help")) {
      eprintln(LANG(usage_es, usage_en), args[0]);
      return EXIT_SUCCESS;
    } else if (strn_eq(arg, "-I", 2) /* Ignore -isystem, -idirafter */) {
      char* path = arg[2]? arg + 2: (j + 1 < argc)? argv[j + 1]: "";
      if (path[0] is '\0' or path[0] is '-') {
        eprintln(LANG("Falta el valor para la opción %s.",
                      "Missing value for %s option."),
                 arg);
        return EINVAL;
      }
      append_path(&include_context.paths, path, strlen(path));
    } else if (str_eq(arg, "-iquote")) {
      char* path = (j + 1 < argc)? argv[j + 1]: "";
      if (path[0] is '\0' or path[0] is '-') {
        eprintln(LANG("Falta el valor para la opción %s.",
                      "Missing value for %s option."),
                 arg);
        return EINVAL;
      }
      append_path(&include_context.paths_quote, path, strlen(path));
    }
    args[i++] = arg;
  }
  assert(i <= argc);

  if (not file_name) {
    eprintln(LANG("Falta el nombre de fichero.", "Missing file name."));
    return ENOENT;
  }

  const char* file_dir_name_end = strrchr(file_name, '/');
  if (file_dir_name_end) {
    prepend_path(&include_context.paths_quote,
                 file_name,
                 (size_t)(file_dir_name_end - file_name));
  }

  if (cc[0] is_not 0) { // Only add options if `cc` is not "".
    mut_Byte_array cmd = {0};
    auto destruct_Byte_array(&cmd);

    for (size_t j = 0; j < i; ++j) {
      if (j is_not 0) push_str(&cmd, " ");
      push_str(&cmd, args[j]);
    }
    if (file_dir_name_end) {
      push_str(&cmd, " -iquote ");
      append_Byte_array(&cmd, (Byte_array_slice){
          B(file_name), B(file_dir_name_end)
        });
    }

    FILE* cc_stdin = popen(as_c_string(&cmd), "w");
    if (cc_stdin) {
      return_code = include(file_name, cc_stdin, &include_context, options);
      if (return_code is_not EXIT_SUCCESS) {
        pclose(cc_stdin);
      } else {
        return_code = pclose(cc_stdin);
        // https://www.man7.org/linux/man-pages/man3/wait.3p.html
        if (WIFEXITED(return_code)) return_code = WEXITSTATUS(return_code);
        else if (WIFSIGNALED(return_code)) return_code = 111;
        else if (WIFSTOPPED (return_code)) return_code = 112;
        else                               return_code = 113;
      }
    } else {
      perror(as_c_string(&cmd));
      return_code = errno;
    }
  } else {
    return_code = include(file_name, stdout, &include_context, options);
  }

  fflush(stdout);

  return return_code;
}
