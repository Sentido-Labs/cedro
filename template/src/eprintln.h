/* -*- coding: utf-8 c-basic-offset: 4 tab-width: 4 indent-tabs-mode: nil -*-
 * vi: set et ts=4 sw=4: */
/** \file */
/** \mainpage
 *
 * Distributed under the Apache License 2.0.
 *
 * \author Alberto González Palomo <https://sentido-labs.com>
 * \copyright 2022 Alberto González Palomo
 */

#include <stdio.h>
#include <stdarg.h>

/** Print a message to `stderr`, with a newline character at the end. */
static void
eprintln(const char * const fmt, ...)
{
    va_list args;
    va_start(args, fmt); vfprintf(stderr, fmt, args); putc('\n', stderr);
    va_end(args);
}

/** Print a message to `stdout`, with a newline character at the end. */
static void
println(const char * const fmt, ...)
{
    va_list args;
    va_start(args, fmt); vprintf(fmt, args); putc('\n', stdout);
    va_end(args);
}

/** Print a message to `stderr`. */
static void
eprint(const char * const fmt, ...)
{
    va_list args;
    va_start(args, fmt); vfprintf(stderr, fmt, args);
    va_end(args);
}

/** Print a message to `stdout`. */
static void
print(const char * const fmt, ...)
{
    va_list args;
    va_start(args, fmt); vprintf(fmt, args);
    va_end(args);
}
