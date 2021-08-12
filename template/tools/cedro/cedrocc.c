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

#define USE_CEDRO_AS_LIBRARY
#include "cedro.c"

#include <unistd.h>

static const char* const
usage_es =
    "Uso: cedrocc [opciones] <fichero.c> [ fichero2.o … ]\n"
    "  Ejecuta Cedro en el primer nombre de fichero que acabe en «.c»,\n"
    "  y compila el resultado con «%s» mas los otros argumentos.\n"
    "    cedrocc -o fichero fichero.c\n"
    "    cedro fichero.c | cc -x c - -o fichero\n"
    "  Se puede especificar el compilador, p.ej. gcc:\n"
    "    CEDRO_CC='gcc -x c -' cedrocc …\n"
    "  Para depuración, esto escribe el código que iría entubado a cc,\n"
    "  en stdout:\n"
    "    CEDRO_CC='' cedrocc …"
    ;
static const char* const
usage_en =
    "Usage: cedrocc [options] <file.c> [ file2.o … ]\n"
    "  Runs Cedro on the first file name that ends with “.c”,\n"
    "  and compiles the result with “%s” plus the other arguments.\n"
    "    cedrocc -o fichero fichero.c\n"
    "    cedro fichero.c | cc -x c - -o fichero\n"
    "  You can specify the compiler, e.g. gcc:\n"
    "    CEDRO_CC='gcc -x c -' cedrocc …\n"
    "  For debugging, this writes the code that would be piped into cc,\n"
    "  into stdout instead:\n"
    "    CEDRO_CC='' cedrocc …"
    ;

typedef size_t mut_size_t, * mut_size_t_mut_p, * const mut_size_t_p;
typedef const size_t * size_t_mut_p, * const size_t_p;
DEFINE_ARRAY_OF(size_t, 0, {});
TYPEDEF_STRUCT(IncludePaths, {
    mut_Byte_array text;
    mut_size_t_array lengths;
  });
void
init_IncludePaths(mut_IncludePaths_p _, size_t initial_capacity)
{
  init_Byte_array(&_->text, initial_capacity * 40);
  init_size_t_array(&_->lengths, initial_capacity);
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
                    (Byte_array_slice){ (Byte_p)path, (Byte_p)path + len });
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
  size_t error = 0;

  Byte_mut_p start = start_of_Byte_array(&_->text);
  while (position is_not _->lengths.len) {
    size_t len = *get_size_t_array(&_->lengths, position);
    if (mem_eq(start, B(file_name), len)) return (Result_size_t){ 0, position };
    start += len;
    ++position;
  }

  return (Result_size_t){ 1, position };
}

TYPEDEF_STRUCT(SourceFile, {
    mut_Byte_array path;
    mut_Byte_array src;
    mut_Marker_array markers;
  });
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
    if (cursor->path.len is path.len &&
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

  mut_Byte_array path_buffer;
  init_Byte_array(&path_buffer, 256);
  size_t i = len_IncludePaths(_);
  while (i is_not 0) {
    --i;
    path_buffer.len = 0;
    append_Byte_array(&path_buffer, get_IncludePaths(_, i));
    push_Byte_array(&path_buffer, '/');
    append_Byte_array(&path_buffer, path);
    if (access(as_c_string(&path_buffer), F_OK) == 0) {
      result->len = 0;
      append_Byte_array(result, bounds_of_Byte_array(&path_buffer));
      found = true;
      break;
    }
  }
  destruct_Byte_array(&path_buffer);

  return found;
}

/**
   Returns either `EXIT_SUCCESS` (that is, `0`),
   an error code as defined in errno.h
   (“Values for errno are now required to be distinct positive values”),
   or `-1` if there was no error but the included file does not contain
   the Cedro `#pragma` so it does not need to be expanded in place.
 */
static int
include(const char* file_name,
        Byte level,
        mut_IncludePaths_p include_paths,
        mut_IncludePaths_p include_paths_quote,
        FILE* cc_stdin)
{
  if (level > 10) {
    eprintln(LANG("Error: demasiada recursión de «include» en: %s",
                  "Error: too much include recursion at: %s"),
             file_name);
    return EINVAL;
  }

  int return_code = EXIT_SUCCESS;

  Options options;
  options @
      .apply_macros           = true,
      .escape_ucn             = false,
      .discard_comments       = false,
      .discard_space          = false,
      .insert_line_directives = true;

  mut_Marker_array markers;
  init_Marker_array(&markers, 8192);
  auto destruct_Marker_array(&markers);

  mut_Byte_array src = {0};
  auto destruct_Byte_array(&src);
  int err = read_file(&src, file_name);
  if (err) {
    print_file_error(err, file_name, &src);
    return err;
  } else {
    parse(&src, &markers);
    if (level is_not 0) {
      if (markers.len is 1) {
        // Included file is not a Cedro file.
        return -1;
      } else {
        fprintf(cc_stdin, "\n#line 1 \"%s\"\n", file_name);
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

    size_t a = 0, b = 0;
    mut_Marker_mut_p m = start_of_mut_Marker_array(&markers);
    mut_Marker_mut_p end = end_of_mut_Marker_array(&markers);
    while (m is_not end and return_code is EXIT_SUCCESS) {
      if (m->token_type is T_PREPROCESSOR) {
        Byte_array_mut_slice content = slice_for_marker(&src, m);
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
        } else {
          content.end_p = content.start_p;
        }

        if (content.end_p > content.start_p) {
          b = index_Marker_array(&markers, m);
          unparse(Marker_array_slice_from(&markers, a, b),
                  &src, original_src_len, file_name, options, cc_stdin);
          a = b;
          mut_Byte_array s = {0};
          auto destruct_Byte_array(&s);
          if ((quoted_include &&
               find_include_file(include_paths_quote, content, &s)) ||
              find_include_file(include_paths, content, &s)) {
            // TODO: check #define guards?
            size_t previous_len = len_IncludePaths(include_paths_quote);
            const char* file_dir_name_end = strrchr(as_c_string(&s), '/');
            if (file_dir_name_end) {
              append_path(include_paths_quote, as_c_string(&s),
                          (size_t)(file_dir_name_end - as_c_string(&s)));
            }
            return_code = include(as_c_string(&s),
                                  level + 1,
                                  include_paths,
                                  include_paths_quote,
                                  cc_stdin);
            truncate_IncludePaths(include_paths_quote, previous_len);
            if (return_code is -1) {
              // No error, but the included file is a plain C file.
              return_code = EXIT_SUCCESS;
            } else {
              fprintf(cc_stdin, "\n#line %lu \"%s\"\n",
                      original_line_number(m->start, &src), file_name);
              a += (a + 1 < markers.len)? 2: 1;// Skip LF if present.
            }
          }
        }
      }
      ++m;
    }
    b = index_Marker_array(&markers, end);
    unparse(Marker_array_slice_from(&markers, a, b),
            &src, original_src_len, file_name, options, cc_stdin);
  }

  return return_code;
}

int main(int argc, char* argv[])
{
  int return_code = EXIT_SUCCESS;

  char* cc = getenv("CEDRO_CC");
  if (cc) {
    eprintln(LANG("Usando CEDRO_CC='%s'\n", "Using CEDRO_CC='%s'\n"), cc);
  } else {
    cc = "cc -x c -";
  }

  char* file_name = NULL;

  mut_IncludePaths include_paths, include_paths_quote;
  init_IncludePaths(&include_paths,       10);
  init_IncludePaths(&include_paths_quote, 10);
  auto destruct_IncludePaths(&include_paths);
  auto destruct_IncludePaths(&include_paths_quote);

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
    if (str_eq(arg, "-h") or str_eq(arg, "--help")) {
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
      append_path(&include_paths, path, strlen(path));
    } else if (str_eq(arg, "-iquote")) {
      char* path = (j + 1 < argc)? argv[j + 1]: "";
      if (path[0] is '\0' or path[0] is '-') {
        eprintln(LANG("Falta el valor para la opción %s.",
                      "Missing value for %s option."),
                 arg);
        return EINVAL;
      }
      append_path(&include_paths_quote, path, strlen(path));
    }
    args[i++] = arg;
  }
  assert(i <= argc);

  if (not file_name) {
    eprintln(LANG("Falta el nombre de fichero.", "Missing file name."));
    return ENOENT;
  }

  if (cc[0] is_not 0) { // Only add options if `cc` is not "".
    size_t length = 0;
    for (size_t j = 0; j < i; ++j) {
      length += (j is 0? 0: 1) + strlen(args[j]);
    }

    ++length; // Make space for the zero terminator.
    char* cmd = malloc(length);
    auto free(cmd);
    cmd[0] = 0;
    // Quadratic performance, but the string is small anyway.
    // Can be optimized by keeping track of the offset for the next write.
    for (size_t j = 0; j < i; ++j) {
      if (j is_not 0) strncat(cmd, " ", length);
      strncat(cmd, args[j], length);
    }

    const char* file_dir_name_end = strrchr(file_name, '/');
    if (file_dir_name_end) {
      prepend_path(&include_paths_quote,
                   file_name,
                   (size_t)(file_dir_name_end - file_name));
    }

    FILE* cc_stdin = (cmd[0] is 0)? stdout: popen(cmd, "w");
    if (cc_stdin) {
      return_code = include(file_name,
                            0,
                            &include_paths,
                            &include_paths_quote,
                            cc_stdin);
      if (return_code is_not EXIT_SUCCESS) {
        if (cc_stdin is_not stdout) pclose(cc_stdin);
      } else {
        if (cc_stdin is_not stdout) return_code = pclose(cc_stdin);
      }
    } else {
      perror(cmd);
      return_code = errno;
    }
  } else {
    return_code = include(file_name,
                          0,
                          &include_paths,
                          &include_paths_quote,
                          stdout);
  }

  fflush(stdout);

  return return_code;
}
