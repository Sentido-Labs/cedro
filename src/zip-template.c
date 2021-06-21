/* -*- coding: utf-8 c-basic-offset: 2 tab-width: 2 indent-tabs-mode: nil -*- */
/** \file */
/** \mainpage
 * Minimal zip utility.
 *
 * \author Alberto González Palomo https://sentido-labs.com
 * \copyright ©2021 Alberto González Palomo https://sentido-labs.com
 *
 * Created: 2021-06-18 21:42
 */

#pragma Cedro 1.0

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

#include <sys/stat.h> // stat()
#include <dirent.h> // opendir(), readdir(), closedir()

TYPEDEF(FileName, mut_Byte_array);
DEFINE_ARRAY_OF(FileName, 0, {
    while (cursor != end) destruct_Byte_array(cursor++);
  });

void collect_file_names(const char* directory_path, mut_FileName_array_p names)
{
  mut_Byte_array name = new_Byte_array(256);
  auto destruct_Byte_array(&name);

  DIR* dir = opendir(directory_path);
  if (not dir) {
    eprint(LANG("Fallo al abrir el directorio «%s»",
                "Failed to open the directory “%s”"),
           directory_path);
    perror(" ");
    return;
  }
  auto closedir(dir);

  struct dirent* entry;
  while ((entry = readdir(dir))) {
    if (str_eq(".", entry->d_name) or str_eq("..", entry->d_name)) continue;
    name.len = 0;
    &name @push_str... (directory_path), ("/"), (entry->d_name);
    struct stat entry_stat;
    stat(as_c_string(&name), &entry_stat);
    if (errno) {
      eprint(LANG("Fallo al leer el fichero o directorio «%s»",
                  "Failed to read the file or directory “%s”"),
             as_c_string(&name));
      perror(" ");
      return;
    } else if (S_ISDIR(entry_stat.st_mode)) {
      collect_file_names(as_c_string(&name), names);
    } else if (S_ISREG(entry_stat.st_mode)) {
      push_FileName_array(names, move_Byte_array(&name));
    }
  }
  if (errno) {
    eprint(LANG("Fallo al leer el directorio «%s»",
                "Failed to read the directory “%s”"),
           directory_path);
    perror("");
    return;
  }
}

#define init(...)     mz_zip_writer_init_heap_v2(__VA_ARGS__)
#define add(...)      mz_zip_writer_add_mem(__VA_ARGS__)
#define finalize(...) mz_zip_writer_finalize_heap_archive(__VA_ARGS__)
#define end(...)      mz_zip_writer_end(__VA_ARGS__)

int main(int argc, char* argv[])
{
  int result = EXIT_SUCCESS;

  if (argc is_not 3) {
    eprintln(LANG("Uso: %s <resultado.zip> <directorio>",
                  "Usage: %s <result.zip> <directory>"));
    return 1;
  }

  const char* const zip_file_name  = argv[1];
  const char* const directory_name = argv[2];

  mut_FileName_array names = new_FileName_array(20);
  auto destruct_FileName_array(&names);
  collect_file_names(directory_name, &names);

  mz_zip_archive zip_archive = {0};
  mz_uint32 zip_flags = MZ_DEFAULT_STRATEGY;
  if (not init(&zip_archive, 0, 0, zip_flags)) {
    eprintln(LANG("Fallo al preparar el archivo zip.",
                  "Failed to initialize zip archive."));
    result = EIO;
    goto exit;
  }
  auto end(&zip_archive);

  mz_uint32 level = MZ_BEST_COMPRESSION;
  for (size_t i = 0; i < names.len; ++i) {
    FilePath file_path = as_c_string(get_mut_FileName_array(&names, i));
    eprintln(" + %s", file_path);

    mut_Byte_array buffer;
    result = read_file(&buffer, file_path);
    if (result is_not EXIT_SUCCESS) {
      goto exit;
    }
    auto destruct_Byte_array(&buffer);

    if (not add(&zip_archive, file_path, buffer.items, buffer.len, level)) {
      eprintln(LANG("Fallo al añadir contenido al archivo zip.",
                    "Failed to add content to the zip archive."));
      result = EIO;
      goto exit;
    }
  }
  mut_Byte_mut_p zip_file      = 0;
  size_t         zip_file_size = 0;
  if (not finalize(&zip_archive, (void**)&zip_file, &zip_file_size)) {
    eprintln(LANG("Fallo al finalizar el archivo zip.",
                  "Failed to finalize the zip archive."));
  }

  mut_File_p output = fopen(zip_file_name, "w");
  if (not output) {
    eprint(LANG("Fallo al abrir el fichero «%s»",
                "Failed to open the file “%s”"),
           zip_file_name);
    perror(" ");
    result = errno;
    goto exit;
  }
  fwrite(zip_file, 1, zip_file_size, output);
  fclose(output);

exit:
  return result;
}
