/* -*- coding: utf-8 c-basic-offset: 4 tab-width: 4 indent-tabs-mode: nil -*-
 * vi: set et ts=4 sw=4: */
/** \file */
/** \mainpage
 * {Template} 0.1a
 *
 * Just an example of using Cedro together with some utility functions,
 * that you can use as base for new programs.
 *
 * Modify this text to match your program, add your name to the
 * copyright statement.
 * This project template is distributed under the MIT license,
 * so you can change it to any license you want.
 *
 * It includes my customized hash table library derived from
 * [khash](http://attractivechaos.github.io/klib/#About),
 * and also Josh Baker’s [btree.c](https://github.com/tidwall/btree.c).
 *
 *   - [main](main_8c.html)
 *   - [hash-table](hash-table_8h.html)
 *   - [btree](btree_8c.html)
 *
 * TODO: put license here, for instance GPL or MIT.
 * You can get help picking one at: https://choosealicense.com/
 *
 * The C logo ([doc/logo.png](logo.png)) was made by
 * [Alberto González Palomo](https://sentido-labs.com).
 *
 * The copy of Cedro under `tools/cedro/` is under the Apache License 2.0,
 * and anyway it does not affect your code if you do not include or link it
 * into your program.
 * Here it is used only to process the source code, and the result
 * is of course not a derivative work of Cedro.
 *
 * \author {Author}
 * \copyright {year} {Author}
 */

#include <stdio.h>
#include <stdlib.h>
#ifdef __UINT8_C
#include <stdint.h>
#else
/* _SYS_INT_TYPES_H: Solaris 8, gcc 3.3.6 */
#ifndef _SYS_INT_TYPES_H
typedef unsigned char uint8_t;
#endif
#endif

#include <stdbool.h>
#include <string.h>
#define SIZE_ERROR (size_t)(-1)
#define mem_eq(a, b, len) (0 == memcmp(a, b, len))
#define str_eq(a, b)      (0 == strcmp(a, b))
#define strn_eq(a, b, n)  (0 == strncmp(a, b, n))
#include <stdarg.h>
#include <assert.h>
#include <errno.h>

#pragma Cedro 1.0

// Custom hash table from khash with a slightly different API:
#include <hash-table.h>

#include <btree.c-0.2.1/btree.h>

DEFINE_HASH_TABLE_STR(unsigned int, StringCount);
struct string_count {
    char* arg;
    size_t count;
};
static int
string_count_cmp(const void* a, const void* b, void* udata)
{
    return strcmp(((const struct string_count*) a)->arg,
                  ((const struct string_count*) b)->arg);
}

#define LANG(es, en) (strncmp(getenv("LANG"), "es", 2) == 0? es: en)
const char* const usage_es =
    "Uso: {template} <argumento> [<argumento> ...]\n"
    "  Imprime los argumentos dados."
    ;
const char* const usage_en =
    "Usage: {template} <argument> [<argument> ...]\n"
    "  Prints the given arguments."
    ;

#include "eprintln.h"

bool
string_count_print(const void* a, void* udata)
{
    const struct string_count* _ = a;
    FILE* out = (FILE*)udata;
    fprintf(out,
            _->count == 1?
            LANG("«%s» aparece %lu vez.\n",
                 "“%s” appears %lu time.\n"):
            LANG("«%s» aparece %lu veces.\n",
                 "“%s” appears %lu times.\n"),
            _->arg, _->count);
    return true;
}

int
main(int argc, char* argv[])
{
    int err = 0;
    if (argc == 1) {
        eprintln(LANG(usage_es, usage_en));
        err = 1;
        return err;
    }

    const char * const format_string =
            LANG("Me has dado %d argumentos.\n",
                 "You gave me %d arguments.\n");
    size_t length = (size_t)snprintf(NULL, 0, format_string, argc - 1);
    if (length == SIZE_ERROR) {
        perror("");
        err = errno;
        return err;
    }
    char* message = malloc(length);
    auto free(message);
    if (SIZE_ERROR == snprintf(message, length, format_string, argc - 1)) {
        perror("");
        err = errno;
        return err;
    }

    eprintln(LANG("Mensaje: %s",
                  "Message: %s"),
             message);

    StringCount_t arg_count = {0};
    auto destruct_StringCount(arg_count);

    struct btree* btree = btree_new(sizeof(struct string_count), 0,
                                    string_count_cmp, NULL);
    auto btree_free(btree);

    char* end;
    for (int i = 1; i < argc; ++i) {
        char* arg = argv[i];

        // Store arg in hash table, count appearances:
        int absent = 0; // Some functions store here whether the key was found.
        unsigned int count = 0;
        khint_t iterator = get_StringCount(&arg_count, arg);
        if (iterator == end_StringCount(&arg_count)) {
            iterator = put_StringCount(&arg_count, arg, &absent);
            // Do not care about absent, because we only call this
            // if the key was not found. Therefore absent is always true.
        } else {
            count = arg_count.vals[iterator];
        }
        ++count;
        arg_count.vals[iterator] = count;

        // Now store in the tree, in a similar way:
        count = 0;
        struct string_count* string_count_p =
                btree_get(btree, &(struct string_count){ .arg = arg });
        if (string_count_p) count = string_count_p->count;
        ++count;
        btree_set(btree, &(struct string_count){ .arg = arg, .count = count });

        long value = strtol(arg, &end, 10);
        if (!errno && end == arg + strlen(arg)) {
            eprintln(LANG(" %d. Entero decimal: %ld",
                          " %d. Decimal integer: %ld"),
                     i, value);
        } else {
            value = strtol(arg, &end, 16);
            if (!errno && end == arg + strlen(arg)) {
                eprintln(LANG(" %d. Entero hexadecimal: 0x%04X",
                              " %d. Hexadecimal integer: 0x%04X"),
                         i, value);
            } else {
                float value = strtof(arg, &end);
                if (!errno && end == arg + strlen(arg)) {
                    eprintln(LANG(" %d. Número de coma flotante: %f",
                                  " %d. Floating point number: %f"),
                             i, value);
                } else {
                    eprintln(LANG(" %d. Cadena: \"%s\"",
                                  " %d. String: \"%s\""),
                             i, arg);
                }
            }
        }
    }

    eprintln(LANG("\nCuentas al usar la tabla «hash»:",
                  "\nCounts using the hash table:"));
    for (unsigned int i = 0; i < end_StringCount(&arg_count); ++i) {
        if (exists_StringCount(&arg_count, i)) {
            const char* key = arg_count.keys[i];
            unsigned int value = arg_count.vals[i];
            eprintln(value == 1?
                     LANG("«%s» aparece %d vez.",
                          "“%s” appears %d time."):
                     LANG("«%s» aparece %d veces.",
                          "“%s” appears %d times."),
                     key, value);
        }
    }

    eprintln(LANG("\nCuentas al usar el árbol «btree»:",
                  "\nCounts using the btree:"));
    btree_ascend(btree,
                 &(struct string_count){ .arg = "" },
                 string_count_print, stderr);

    return err;
}
