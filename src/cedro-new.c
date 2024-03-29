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
 * Created: 2021-06-18 08:33
 */

#pragma Cedro 1.0 #embed

/* _POSIX_C_SOURCE is needed for getline(). */
#define _POSIX_C_SOURCE 200809L

#if defined(__GNUC__)
  // Ensure we get the 64-bit variants of the CRT's file I/O calls
  #ifndef _FILE_OFFSET_BITS
    #define _FILE_OFFSET_BITS 64
  #endif
  #ifndef _LARGEFILE64_SOURCE
    #define _LARGEFILE64_SOURCE 1
  #endif
#endif

// Get Cedro’s utility functions and typedefs.
// This is overkill, but negligible in this case.
#define USE_CEDRO_AS_LIBRARY
#include "cedro.c"

#define Byte mz_Byte
#include "../lib/miniz/miniz.c"
#undef Byte

#include <sys/stat.h> // mkdir(), stat()
#include <stdio.h> // getline()
bool get_line(mut_Byte_array_p answer, FILE* input)
{
  ssize_t len = getline((char**)&answer->start, &answer->capacity, input);
  if (len is_not -1) {
    assert(len is_not 0); // getline() includes the newline character.
    answer->len = (size_t)(len - 1);
    if (answer->capacity is 0) answer->capacity = answer->len;
    return true;
  } else {
    return false;
  }
}
#include <wctype.h> // wint_t, towupper()

const char* const usage_es =
    "Uso: cedro-new [opciones] <nombre>\n"
    "  Crea un directorio llamado <nombre>/ con la plantilla.\n"
    "  -h, --help        Muestra este mensaje.\n"
    "  -i, --interactive Pregunta por los nombres de programa y proyecto.\n"
    "                    Si no, se eligen a partir del nombre del directorio."
    ;
const char* const usage_en =
    "Usage: cedro-new [options] <name>\n"
    "  Creates a directory named <name>/ with the template.\n"
    "  -h, --help        Shows this message.\n"
    "  -i, --interactive Asks for the names for command and project.\n"
    "                    Otherwise, they are derived from the directory name."
    ;

Byte template_zip[] = {
#embed "../template.zip"
};

#define init(...)                mz_zip_reader_init_mem(__VA_ARGS__)
#define get_num_files(...)       mz_zip_reader_get_num_files(__VA_ARGS__)
#define file_stat(...)           mz_zip_reader_file_stat(__VA_ARGS__)
#define is_file_a_directory(...) mz_zip_reader_is_file_a_directory(__VA_ARGS__)
#define extract(...)             mz_zip_reader_extract_file_to_heap(__VA_ARGS__)
#define end(...)                 mz_zip_reader_end(__VA_ARGS__)

static void
uppercase(mut_Byte_array_p _, size_t letter_count_from_start)
{
  Byte_mut_p cursor = start_of_Byte_array(_);
  Byte_mut_p    end = end_of_Byte_array(_);
  while (cursor is_not end and letter_count_from_start-- is_not 0) {
    wint_t u = 0;
    size_t cursor_index = (size_t)(cursor - start_of_Byte_array(_));
    UTF8Error err = UTF8_NO_ERROR;
    cursor = decode_utf8(cursor, end, &u, &err);
    if (utf8_error(err, cursor_index)) {
      eprintln(LANG("Error al descodificar UTF-8 en capitalize(): %s",
                    "Error when deocding UTF-8 in capitalize(): %s"),
               error_buffer);
      error_buffer[0] = 0;
      return;
    }
    wint_t capital_letter = towupper(u);
    if (capital_letter is u) {
      /* `towupper()` only converts characters from the current locale,
         and some times not even that. Here is a backup for some alphabets: */
      if      (in(0x00E0,u,0x00FD)) capital_letter &= 0xFFDF;
      else if        (u is 0x00FF)  capital_letter  = 0x0178;
      else if (in(0x0101,u,0x024F)) capital_letter -= 1;
      else if (in(0x03B1,u,0x03C9)) capital_letter &= 0xFFDF;
      else if (in(0x0430,u,0x045F)) capital_letter &= 0xFFDF;
      else if (in(0x0461,u,0x0527)) capital_letter -= 1;
      else if (in(0x0561,u,0x0586)) capital_letter &= 0xFFCF;
      else if (in(0x1E01,u,0x1EFF)) capital_letter -= 1;
      else if (in(0x1F00,u,0x1FB3)) capital_letter |= 0x0008;
    }
    if (capital_letter is_not u) {
      mut_Byte utf8[4] = {0};
      size_t len;
      if        ((capital_letter & 0xFFFFFF80) is 0) {
        len = 1;
        utf8[0] = (Byte)capital_letter;
      } else if ((capital_letter & 0xFFFFF800) is 0) {
        len = 2;
        utf8[1] = 0x80 | (Byte)((capital_letter      ) & 0x3F);
        utf8[0] = 0xC0 | (Byte)((capital_letter >>  6) & 0x1F);
      } else if ((capital_letter & 0xFFFF0000) is 0) {
        len = 3;
        utf8[2] = 0x80 | (Byte)((capital_letter      ) & 0x3F);
        utf8[1] = 0x80 | (Byte)((capital_letter >>  6) & 0x3F);
        utf8[0] = 0xE0 | (Byte)((capital_letter >> 12) & 0x0F);
      } else if (capital_letter <= 0x0010FFFF) {
        len = 4;
        utf8[3] = 0x80 | (Byte)((capital_letter      ) & 0x3F);
        utf8[2] = 0x80 | (Byte)((capital_letter >>  6) & 0x3F);
        utf8[1] = 0x80 | (Byte)((capital_letter >> 12) & 0x3F);
        utf8[0] = 0xE0 | (Byte)((capital_letter >> 24) & 0x0F);
      } else {
        error(LANG("Mayúscula no válida: 0x%4X",
                   "Invalid capital letter: 0x%4X"),
              capital_letter);
        return;
      }
      Byte_array_slice capital_letter_utf8 = { &utf8[0], &utf8[len] };
      splice_Byte_array(_, cursor_index, len, NULL, capital_letter_utf8);
      cursor = start_of_Byte_array(_) + cursor_index + len;
      end    =   end_of_Byte_array(_);
    }
  }
}

int main(int argc, char* argv[])
{
  int result = EXIT_SUCCESS;

  bool interactive = false;
  FilePath project_path = NULL;

  for (int i = 1; i < argc; ++i) {
    char* arg = argv[i];
    if (arg[0] is '-') {
      if        (str_eq("-i", arg) or str_eq("--interactive", arg)) {
        interactive = true;
      } else if (str_eq("-h", arg) or str_eq("--help", arg)) {
        eprintln(LANG(usage_es, usage_en));
        return EXIT_SUCCESS;
      } else {
        eprintln(LANG("Opción desconocida: %s\n%s",
                      "Unknown option: %s\n%s"),
                 arg, LANG(usage_es, usage_en));
        return EDOM;
      }
    } else {
      if (not project_path) {
        project_path = argv[i];
      } else {
        eprintln(LANG("Sólo se puede especificar un directorio: %s\n%s",
                      "Only one directory can be specified: %s\n%s"),
                 arg, LANG(usage_es, usage_en));
        return EDOM;
      }
    }
  }

  if (not project_path or project_path[0] is 0) {
    eprintln(LANG("Falta el nombre para el directorio.\n%s",
                  "Missing directory name.\n%s"),
             LANG(usage_es, usage_en));
    return EDOM;
  }

  struct stat project_dir_stat;
  if (stat((const char *)project_path, &project_dir_stat) is 0) {
    eprintln(LANG("El directorio de projecto ya existe: «%s»",
                  "The project directory already exists: “%s”"),
             project_path);
    return EEXIST;
  }

  mz_uint32 zip_flags = 0; // mz_zip_flags
  mz_zip_archive zip_archive = {0};
  if (not init(&zip_archive, template_zip, sizeof(template_zip), zip_flags)) {
    eprintln(LANG("Fallo al abrir el archivo zip en memoria.",
                  "Failed to open built-in zip archive."));
    return EIO;
  }
  auto end(&zip_archive);

  mut_Byte_array command_name = {0}, project_name = {0}, constant_name = {0};
  auto destruct_Byte_array(&command_name);
  auto destruct_Byte_array(&project_name);
  auto destruct_Byte_array(&constant_name); // All caps, for naming constants.

  push_str(&command_name,
           strrchr(project_path, '/')?
           strrchr(project_path, '/') + 1: project_path);

  append_Byte_array(&project_name, bounds_of_Byte_array(&command_name));
  uppercase(&project_name, 1);
  for (mut_Byte_mut_p cursor = start_of_mut_Byte_array(&project_name),
         end = end_of_mut_Byte_array(&project_name);
       cursor is_not end; ++cursor) {
    if (*cursor is '-' or *cursor is '_') *cursor = ' ';
  }

  if (interactive) {
    eprint(LANG("¿Nombre del ejecutable? (implícito: %s): ",
                "Executable file name? (default: %s): "),
           as_c_string(&command_name));
    mut_Byte_array answer = {0};
    auto destruct_Byte_array(&answer);
    if (not get_line(&answer, stdin)) {
      perror("");
      return result;
    }
    if (answer.len) {
      command_name.len = 0;
      append_Byte_array(&command_name, bounds_of_Byte_array(&answer));
    }

    eprint(LANG("¿Nombre completo del proyecto? (implícito: %s): ",
                "Full project name? (default: %s): "),
           as_c_string(&project_name));
    if (not get_line(&answer, stdin)) {
      perror("");
      return result;
    }
    if (answer.len) {
      project_name.len = 0;
      append_Byte_array(&project_name, bounds_of_Byte_array(&answer));
    }
  }

  append_Byte_array(&constant_name, bounds_of_Byte_array(&command_name));
  uppercase(&constant_name, ~0);
  if (constant_name.start[0] >= '0' and constant_name.start[0] <= '9') {
    constant_name.start[0] = '_'; // Ensure valid C identifier.
  }
  for (mut_Byte_mut_p cursor = start_of_mut_Byte_array(&constant_name) + 1,
         end = end_of_mut_Byte_array(&constant_name);
       cursor is_not end; ++cursor) {
    if ((*cursor < 'A' or *cursor > 'Z') and
        (*cursor < '0' or *cursor > '9') and
        *cursor is_not '_') *cursor = '_';
  }

  char user_name [255 + 1] = {0};
  char user_email[255 + 1] = {0};
  FILE* git_stdout =
      popen("git config user.name 2>/dev/null", "r");
  if (git_stdout) {
    int len = fread(user_name, sizeof(user_name[0]), 255, git_stdout);
    pclose(git_stdout);
    if (len == -1) {
      perror("git config user.name");
      // If this command failed, the next one is not going to work. Give up.
    } else {
      if (len) user_name[len-1] = 0; // There is a LF after the name.
      git_stdout = popen("git config user.email 2>/dev/null", "r");
      if (git_stdout) {
        len = fread(user_email, sizeof(user_email[0]), 255, git_stdout);
        pclose(git_stdout);
        if (len == -1) perror("git config user.email");
        else if (len)  user_email[len-1] = 0; // There is a LF after the email.
      }
    }
  }
  if (user_name [0] == 0) strncpy(user_name,  "Your Name Here",    256);
  if (user_email[0] == 0) strncpy(user_email, "email@example.com", 256);

  mut_Byte_array path = init_Byte_array(256);
  auto destruct_Byte_array(&path);

  for (mz_uint i = 0; i < get_num_files(&zip_archive); i++) {
    mz_zip_archive_file_stat file_stat;
    if (not file_stat(&zip_archive, i, &file_stat)) {
      eprintln(LANG("Fallo al obtener las propiedades del fichero",
                    "Failed to get file properties."));
      return EIO;
    }

    if (is_file_a_directory(&zip_archive, i)) {
      // Skip this, we do not use empty directories for now.
      continue;
    }

    path.len = 0;
    push_str(&path, project_path);
    if (*get_Byte_array(&path, path.len-1) is_not '/') {
      push_Byte_array(&path, '/');
    }
    const char* filename = file_stat.m_filename;
    if (not strn_eq("template/", filename, 9)) {
      eprintln(LANG("Estructura incorrecta del archivo ZIP de plantilla.",
                    "Incorrect template ZIP file structure."));
      return EIO;
    }
    filename += 9;
    push_str(&path, filename);

    mz_uint flags = 0; // From enum mz_zip_flags.
    size_t extracted_size;
    void*  extracted_bytes = extract(&zip_archive,
                                     file_stat.m_filename, &extracted_size,
                                     flags);
    if (not extracted_bytes) {
      eprintln(LANG("Fallo al extraer el archivo: %s",
                    "Failed to extract file: %s"),
               file_stat.m_filename);
      return EIO;
    }
    assert(extracted_size is file_stat.m_uncomp_size);
    auto mz_free(extracted_bytes);

    mut_Byte_p start = start_of_mut_Byte_array(&path);
    mut_Byte_p end   = end_of_mut_Byte_array(&path);
    mut_Byte_mut_p cursor = start;
    while (cursor is_not end) {
      cursor = memchr(cursor, '/', (size_t)(end - cursor));
      if (not cursor) break;
      if (cursor is start) { // This is an absolute path.
        ++cursor;
        continue;
      }
      *cursor = 0;
      struct stat entry_stat;
      if (stat((const char *)start, &entry_stat) is_not 0) {
        if (errno is ENOENT) {
          if (mkdir((const char *)start, 0777) is_not 0) {
            eprint(LANG("Fallo al crear el directorio «%s»",
                        "Failed to create the directory “%s”"),
                   (const char *)start);
            result = errno;
            perror(" ");
            return result;
          }
        } else {
          eprint(LANG("Fallo al leer el fichero o directorio «%s»",
                      "Failed to read the file or directory “%s”"),
                 (const char *)start);
          result = errno;
          perror(" ");
          return result;
        }
      } else if (S_ISDIR(entry_stat.st_mode)) {
        // The directory already exists.
      } else {
        eprintln(LANG("Ya existe y no es un directorio: «%s»",
                      "Already exists and is not a directory: “%s”"),
                 (const char *)start);
        return EEXIST;
      }
      *cursor = '/';
      ++cursor;
    }

    mut_File_p output = fopen(as_c_string(&path), "wb");
    if (not output) {
      eprint(LANG("Fallo al abrir el fichero «%s» para escritura.",
                  "Failed to open the file “%s” for writing."),
             as_c_string(&path));
      result = errno;
      perror(" ");
      return result;
    }

    if (str_eq("template/Makefile",   file_stat.m_filename) or
        str_eq("template/README.md",  file_stat.m_filename) or
        strn_eq("template/src/", file_stat.m_filename,
                strlen("template/src/"))) {
      Byte_p start = (Byte_p)extracted_bytes;
      Byte_p end   = (Byte_p)extracted_bytes + extracted_size;
      Byte_mut_p cursor = start;
      Byte_mut_p previous = cursor;
      while (cursor is_not end and not ferror(output)) {
        cursor = memchr(cursor, '{', (size_t)(end - cursor));
        if (cursor is NULL) { cursor = end; break; }
        if (cursor + 7 < end and mem_eq(cursor, "{#year}", 7)) {
          fwrite(previous, sizeof(Byte), (size_t)(cursor - previous), output);
          struct tm tm = *localtime(&(time_t){time(NULL)});
          fprintf(output, "%d", 1900 + tm.tm_year);
          cursor += 7;
          previous = cursor;
        } else if (cursor + 9 < end and mem_eq(cursor, "{#Author}", 9)) {
          fwrite(previous, sizeof(Byte), (size_t)(cursor - previous), output);
          fprintf(output, "%s <%s>", user_name, user_email);
          cursor += 9;
          previous = cursor;
        } else if (cursor + 11 < end and mem_eq(cursor, "{#Template}", 11)) {
          fwrite(previous, sizeof(Byte), (size_t)(cursor - previous), output);
          fwrite(project_name.start, 1, project_name.len, output);
          cursor += 11;
          previous = cursor;
        } else if (cursor + 11 < end and mem_eq(cursor, "{#template}", 11)) {
          fwrite(previous, sizeof(Byte), (size_t)(cursor - previous), output);
          fwrite(command_name.start, 1, command_name.len, output);
          cursor += 11;
          previous = cursor;
        } else if (cursor + 11 < end and mem_eq(cursor, "{#TEMPLATE}", 11)) {
          fwrite(previous, sizeof(Byte), (size_t)(cursor - previous), output);
          fwrite(constant_name.start, 1, constant_name.len, output);
          cursor += 11;
          previous = cursor;
        } else {
          cursor += 1;
        }
      }
      fwrite(previous, sizeof(Byte), (size_t)(cursor - previous), output);
    } else {
      fwrite(extracted_bytes, sizeof(Byte), extracted_size, output);
    }

    if (ferror(output)) {
      eprint(LANG("Fallo al escribir en el fichero «%s»",
                  "Failed to write to the file “%s”"),
             as_c_string(&path));
      result = errno;
      perror(" ");
      return result;
    }
    fclose(output);
  }

  return result;
}
