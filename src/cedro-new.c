/* -*- coding: utf-8 c-basic-offset: 2 tab-width: 2 indent-tabs-mode: nil -*- */
/** \file */
/** \mainpage
 * Cedro C pre-processor piped through the system’s C compiler, cc.
 *
 * \author Alberto González Palomo https://sentido-labs.com
 * \copyright ©2021 Alberto González Palomo https://sentido-labs.com
 *
 * Created: 2021-06-18 08:33
 */

#pragma Cedro 1.0

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
    "                    Otherwise, they will be derived from the directory name."
    ;

Byte template_zip
#include {template.zip}
;

#define init(...)                mz_zip_reader_init_mem(__VA_ARGS__)
#define get_num_files(...)       mz_zip_reader_get_num_files(__VA_ARGS__)
#define file_stat(...)           mz_zip_reader_file_stat(__VA_ARGS__)
#define is_file_a_directory(...) mz_zip_reader_is_file_a_directory(__VA_ARGS__)
#define extract(...)             mz_zip_reader_extract_file_to_heap(__VA_ARGS__)
#define end(...)                 mz_zip_reader_end(__VA_ARGS__)

static void
capitalize(mut_Byte_array_p _)
{
  Byte_mut_p cursor = start_of_Byte_array(_);
  Byte_p        end = end_of_Byte_array(_);
  wint_t u = decode_utf8(&cursor, end);
  if (error_buffer[0]) return;
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
    Byte_array_slice capital_letter_utf8 = {
      .start_p = &utf8[0],
      .end_p   = &utf8[len]
    };
    splice_Byte_array(_, 0,
                      (size_t)(cursor - start_of_Byte_array(_)), NULL,
                      &capital_letter_utf8);
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

  mut_Byte_array command_name = {0}, project_name = {0};
  auto destruct_Byte_array(&command_name);
  auto destruct_Byte_array(&project_name);

  push_str(&command_name,
           strrchr(project_path, '/')?
           strrchr(project_path, '/') + 1: project_path);

  if (interactive) {
    size_t len;
    char* answer = NULL;
    eprint(LANG("¿Nombre del ejecutable? (implícito: %s): ",
                "Executable file name? (default: %s): "),
           as_c_string(&command_name));
    if (getline(&answer, &len, stdin) is -1) {
      perror("");
      return result;
    }
    // getline() includes the newline character.
    assert(len is_not 0);
    answer[len - 1] = 0;
    command_name.len = 0;
    push_str(&command_name, answer);
    free(answer);

    push_str(&project_name, as_c_string(&command_name));
    capitalize(&project_name);
    answer = NULL;
    eprint(LANG("¿Nombre completo del proyecto? (implícito: %s): ",
                "Full project name? (default: %s): "),
           as_c_string(&project_name));
    if (getline(&answer, &len, stdin) is -1) {
      perror("");
      return result;
    }
    // getline() includes the newline character.
    assert(len is_not 0);
    answer[len - 1] = 0;
    project_name.len = 0;
    push_str(&project_name, answer);
    free(answer);
  } else {
    push_str(&project_name, as_c_string(&command_name));
    capitalize(&project_name);
  }

  char user_name [255 + 1];
  char user_email[255 + 1];
  user_name[0] = user_email[0] = 0;
  FILE* git_stdout = popen("git config user.name", "r");
  if (git_stdout) {
    int len = fread(user_name, sizeof(user_name[0]), 255, git_stdout);
    pclose(git_stdout);
    if (len == -1) {
      perror("git config user.name");
      // If this command failed, the next one is not going to work. Give up.
    } else {
      if (len) user_name[len-1] = 0;
      git_stdout = popen("git config user.email", "r");
      if (git_stdout) {
        len = fread(user_email, sizeof(user_email[0]), 255, git_stdout);
        pclose(git_stdout);
        if (len == -1) perror("git config user.email");
        else if (len)  user_email[len-1] = 0;
      }
    }
  }
  if (user_name [0] == 0) strncpy(user_name,  "Your Name Here",    256);
  if (user_email[0] == 0) strncpy(user_email, "email@example.com", 256);

  mut_Byte_array path = new_Byte_array(256);
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
    if (strn_eq("template/", filename, 9)) filename += 9;
    push_str(&path, filename);

    mz_uint flags = 0;// From enum mz_zip_flags.
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

    mut_File_p output = fopen(as_c_string(&path), "w");
    if (not output) {
      eprint(LANG("Fallo al abrir el fichero «%s» para escritura.",
                  "Failed to open the file “%s” for writing."),
             as_c_string(&path));
      result = errno;
      perror(" ");
      return result;
    }

    if (str_eq("template/Makefile",   file_stat.m_filename) or
        str_eq("template/src/main.c", file_stat.m_filename)) {
      Byte_p start = (Byte_p)extracted_bytes;
      Byte_p end   = (Byte_p)extracted_bytes + extracted_size;
      Byte_mut_p cursor = start;
      Byte_mut_p previous = cursor;
      while (cursor is_not end and not ferror(output)) {
        cursor = memchr(cursor, '{', (size_t)(end - cursor));
        if (cursor is NULL) { cursor = end; break; }
        if (cursor + 6 < end and mem_eq(cursor, "{year}", 6)) {
          fwrite(previous, sizeof(Byte), (size_t)(cursor - previous), output);
          struct tm tm = *localtime(&(time_t){time(NULL)});
          fprintf(output, "%d", 1900 + tm.tm_year);
          cursor += 6;
          previous = cursor;
        } else if (cursor + 8 < end and mem_eq(cursor, "{Author}", 8)) {
          fwrite(previous, sizeof(Byte), (size_t)(cursor - previous), output);
          fprintf(output, "%s <%s>", user_name, user_email);
          cursor += 8;
          previous = cursor;
        } else if (cursor + 10 < end and mem_eq(cursor, "{Template}", 10)) {
          fwrite(previous, sizeof(Byte), (size_t)(cursor - previous), output);
          fwrite(start_of_Byte_array(&project_name), 1, project_name.len, output);
          cursor += 10;
          previous = cursor;
        } else if (cursor + 10 < end and mem_eq(cursor, "{template}", 10)) {
          fwrite(previous, sizeof(Byte), (size_t)(cursor - previous), output);
          fwrite(start_of_Byte_array(&command_name), 1, command_name.len, output);
          cursor += 10;
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
