/* -*- coding: utf-8 c-basic-offset: 2 tab-width: 2 indent-tabs-mode: nil -*-
 * vi: set et ts=2 sw=2: */
/** \file */
/** \mainpage
 * The Cedro C pre-processor has four features:
 * - the *backstitch* `@` operator.
 * - *deferred* resource release.
 * - *block* (or multi-line) macros.
 * - *binary* inclusion.
 *
 * For usage instructions, see [README.en.html](../../README.en.html).
 *
 *   - [Data structures](cedro_8c.html#nested-classes)
 *   - [Macros](cedro_8c.html#define-members)
 *   - [Typedefs](cedro_8c.html#typedef-members)
 *   - [Enumerations](cedro_8c.html#enum-members)
 *   - [Functions](cedro_8c.html#func-members)
 *   - [Variables](cedro_8c.html#var-members)
 *
 * Array utilities, `DEFINE_ARRAY_OF()`: [array.h](array_8h.html)
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
 * Created: 2020-11-25 22:41
 */

// Uncomment this to use `defer` instead of `auto`:
//#define USE_DEFER_AS_KEYWORD

/* In Solaris 8, we need __EXTENSIONS__ for vsnprintf(). */
#define __EXTENSIONS__

#include <stdio.h>
#include <stdlib.h>
#ifdef __UINT8_C
#include <stdint.h>
#else
/* _SYS_INT_TYPES_H: Solaris 8, gcc 3.3.6 */
#ifndef _SYS_INT_TYPES_H
typedef unsigned char uint8_t;
typedef unsigned long uint32_t;
#endif
#endif

#include <stdbool.h>
#include <string.h> // For memcpy(), memmove().
#define mem_eq(a, b, len)   (0 is memcmp(a, b, len))
#define str_eq(a, b)        (0 is strcmp(a, b))
#define strn_eq(a, b, len)  (0 is strncmp(a, b, len))
#include <assert.h>
#include <errno.h>

#include <iso646.h> // and, or, not, not_eq, etc.
#define is_not not_eq
#define is     ==
#define in(min, x, max) (x >= min and x <= max)

#include <sys/resource.h> // rlimit, setrlimit()

#define CEDRO_VERSION "1.0"
/** Versions with the same major number are compatible in that they produce
 * semantically equivalent output: there might be differeces in indentation
 * etc. but will be the same after parsing by the compiler.
 */
#define CEDRO_PRAGMA "#pragma Cedro 1."
#define CEDRO_PRAGMA_LEN 16

typedef size_t SrcIndexType; // Must be enough for the maximum src file size.
typedef uint32_t SrcLenType; // Must be enough for the maximum token length.

#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#ifdef NDEBUG
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif

#define LANG(es, en) (strn_eq(getenv("LANG"), "es", 2)? es: en)

#include "utf8.h"

#include <stdarg.h>
static const size_t error_buffer_size = 256;
static char         error_buffer[256] = {0};
static void
error(const char * const fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  int len = vsnprintf(error_buffer, error_buffer_size, fmt, args);
  assert(len < error_buffer_size);
  va_end(args);
}
static void
error_append(const char* const start, const char* const end)
{
  size_t len = (size_t)(end - start);
  size_t previous_len = strlen(error_buffer);
  assert(previous_len + len < error_buffer_size);
  if (previous_len + len + 1 > error_buffer_size) {
    len = error_buffer_size - 1 - previous_len;
  }
  memcpy(error_buffer + previous_len, start, len);
  error_buffer[previous_len + len] = 0;
}

/** Store the error message corresponding to the error code `err`.
 *
 * If you want to assume that the input is valid UTF-8, you can do this
 * to disable UTF-8 decoding error checking and get a notable boost
 * with optimizing compilers:
 *#define utf8_error(_) false
 *
 * If you want to do it only is specific cases, use `decode_utf8_unchecked()`.
 */
static bool
utf8_error(UTF8Error err, size_t position)
{
  switch (err) {
    case UTF8_NO_ERROR:
      return false;
    case UTF8_ERROR:
      error(LANG("Error descodificando UTF-8 en octeto %lu.",
                 "UTF-8 decode error at byte %lu."),
            position);
      break;
    case UTF8_ERROR_OVERLONG:
      error(LANG("Error UTF-8, secuencia sobrelarga en octeto %lu.",
                 "UTF-8 error, overlong sequence at byte %lu."),
            position);
      break;
    case UTF8_ERROR_INTERRUPTED_1:
    case UTF8_ERROR_INTERRUPTED_2:
    case UTF8_ERROR_INTERRUPTED_3:
      error(LANG("Error UTF-8, secuencia interrumpida en octeto %lu.",
                 "UTF-8 error, interrupted sequence at byte %lu."),
            position);
      break;
    default:
      error(LANG("Error UTF-8 inesperado 0x%02X en octeto %lu.",
                 "UTF-8 error, unexpected 0x%02X at byte %lu."),
            err, position);
  }

  return true;
}

/** Same as `fprintf(stderr, fmt, ...)`
 * but converting UTF-8 characters to Latin-1 if the `LANG` environment
 * variable does not contain `UTF-8`. */
__attribute__ ((format (printf, 1, 2))) // GCC, typecheck format string args
static void
eprint(const char * const fmt, ...)
{
  va_list args;
  va_start(args, fmt);

  if (strstr(getenv("LANG"), "UTF-8")) {
    vfprintf(stderr, fmt, args);
  } else {
    char* buffer;
    char small[512];
    size_t needed =
        (size_t) vsnprintf(small, sizeof(small), fmt, args)
        + 1; // For the zero terminator.
    if (needed <= sizeof(small)) {
      buffer = &small[0];
    } else {
      buffer = malloc(needed);
      if (not buffer) {
        error(LANG("Error de falta de memoria. Se necesitan %lu octetos.\n",
                   "Out of memory error. %lu bytes needed.\n"),
              needed);
        goto exit;
      }
      vsnprintf(buffer, needed, fmt, args);
    }
    const uint8_t* p   = (const uint8_t*)buffer;
    const uint8_t* end = p + needed;
    for (;;) {
      uint32_t u = 0;
      UTF8Error err = UTF8_NO_ERROR;
      p = decode_utf8(p, end, &u, &err);
      if (utf8_error(err, (size_t)(p - (uint8_t*)buffer))) goto exit;
      if (not u) break;
      if ((u & 0xFFFFFF00) is 0) {
        fputc((unsigned char) u, stderr); // Latin-1 / ISO-8859-1 / ISO-8859-15
      } else {
        switch (u) {
          case 0x00002018:
          case 0x00002019: fputc('\'',  stderr); break;
          case 0x0000201C:
          case 0x0000201D: fputc('"',   stderr); break;
          case 0x00002026: fputs("...", stderr); break;
          case 0x00002190: fputs("<-",  stderr); break;
          case 0x00002192: fputs("->",  stderr); break;
          default:         fputc('_',   stderr);
        }
      }
    }
 exit:
    if (buffer is_not &small[0]) free(buffer);
  }

  va_end(args);
}

/* “backward compatiblity reasons”:
 * https://www.doxygen.nl/manual/markdown.html#mddox_code_spans
 */
/** Same as ``fprintf(stderr, fmt, ...) fputc('\n', stderr)``
 * but converting UTF-8 characters to Latin-1 if the `LANG` environment
 * variable does not contain `UTF-8`. */
#define eprintln(...) eprint(__VA_ARGS__), eprint("\n")

/** Expand to the mutable and constant variants for a `typedef`. \n
 * The default, just the type name `T`, is the constant variant
 * and the mutable variant is named `mut_T`, with corresponding \n
 * `T_p`: constant pointer to constant `T`       \n
 * `mut_T_p`: constant pointer to mutable `T`    \n
 * `mut_T_mut_p`: mutable pointer to mutable `T` \n
 * This mimics the usage in Rust, where constant bindings are the default
 * which is a good idea.
 */
#define MUT_CONST_TYPE_VARIANTS(T)              \
  /**/      mut_##T,                            \
    *       mut_##T##_mut_p,                    \
    * const mut_##T##_p;                        \
  typedef const mut_##T T,                      \
    *                   T##_mut_p,              \
    * const             T##_p

/** Parameters set by command line options. */
typedef struct Options {
  /// Apply the macros.
  bool apply_macros;
  /// Escape Unicode® code points (“chararacters”) in identifiers
  /// as universal character names (ISO/IEC 9899:TC3 Annex D).
  bool escape_ucn;
  /// Whether to skip space tokens, or include them in the markers array.
  bool discard_space;
  /// Skip comments, or include them in the markers array.
  bool discard_comments;
  /// Insert `#line` directives in the output, mapping to the original file.
  bool insert_line_directives;
} MUT_CONST_TYPE_VARIANTS(Options);

/** Binary string, `const unsigned char const*`. */
#define B(string) ((const unsigned char * const)string)

/** These token types loosely correspond to those in the C grammar.
 *
 *  Keywords:
 *  https://en.cppreference.com/w/c/keyword
 *
 *  Operator precedence levels:
 *  https://en.cppreference.com/w/c/language/operator_precedence
 */
typedef enum TokenType {
  /** No token, used as marker for uninitialized data. */ T_NONE,
  /** Identifier. See `identifier()`.                 */ T_IDENTIFIER,
  /** Type name: `char, double, enum, float, int,
      long, short, union, void`.
      (c99: `bool, complex, imaginary`)               */ T_TYPE,
  /** Type name: `struct`.                            */ T_TYPE_STRUCT,
  /** Type qualifier: `const, extern, inline,
      register, signed, static, unsigned, volatile`.
      (c99: `restrict`)                               */ T_TYPE_QUALIFIER,
  /** Type qualifier: `auto`.                         */ T_TYPE_QUALIFIER_AUTO,
  /** Type definition: `typedef`.                     */ T_TYPEDEF,
  /** Control flow keyword: `else, if`.               */ T_CONTROL_FLOW_IF,
  /** Control flow keyword: `do, for, while`.         */ T_CONTROL_FLOW_LOOP,
  /** Control flow keyword: `switch`.                 */ T_CONTROL_FLOW_SWITCH,
  /** Control flow keyword: `case`, `default`.        */ T_CONTROL_FLOW_CASE,
  /** Control flow keyword: `break`.                  */ T_CONTROL_FLOW_BREAK,
  /** Control flow keyword: `continue`.               */ T_CONTROL_FLOW_CONTINUE,
  /** Control flow keyword: `return`.                 */ T_CONTROL_FLOW_RETURN,
  /** Control flow keyword: `goto`.                   */ T_CONTROL_FLOW_GOTO,
  /** Control flow, label for `goto`.                 */ T_CONTROL_FLOW_LABEL,
  /** Number, either integer or float.
      See `number()`.                                 */ T_NUMBER,
  /** String including the quotes: `"ABC"`            */ T_STRING,
  /** Character including the apostrophes: ```'A'```  */ T_CHARACTER,
  /** Whitespace, a block of `SP`, `HT`, `LF` or `CR`.
      See [Wikipedia, Basic ASCII control codes](https://en.wikipedia.org/wiki/C0_and_C1_control_codes#Basic_ASCII_control_codes)     */ T_SPACE,
  /** Comment block or line.                          */ T_COMMENT,
  /** Preprocessor directive.                         */ T_PREPROCESSOR,
  /** _Generic keyword.                               */ T_GENERIC_MACRO,
  /** Start of a block: `{`                           */ T_BLOCK_START,
  /** End   of a block: `}`                           */ T_BLOCK_END,
  /** Start of a tuple: `(`                           */ T_TUPLE_START,
  /** End   of a tuple: `)`                           */ T_TUPLE_END,
  /** Start of an array index: `[`                    */ T_INDEX_START,
  /** End   of an array index: `]`                    */ T_INDEX_END,
  /** Invisible grouping of tokens,
      for instance for operator precedence.           */ T_GROUP_START,
  /** End invisible grouping of tokens.               */ T_GROUP_END,
  /** `++ -- () [] . -> (type){list}`            */ T_OP_1,
  /** `++ -- + - ! ~ (type) * & sizeof _Alignof` */ T_OP_2,
  /** `* / %`                                    */ T_OP_3,
  /** `+ -`                                      */ T_OP_4,
  /** `<< >>`                                    */ T_OP_5,
  /** `< <= > >=`                                */ T_OP_6,
  /** `== !=`                                    */ T_OP_7,
  /** `&`                                        */ T_OP_8,
  /** `^`                                        */ T_OP_9,
  /** `|`                                        */ T_OP_10,
  /** `&&`                                       */ T_OP_11,
  /** `||`                                       */ T_OP_12,
  /** `?:`                                       */ T_OP_13,
  /** `= += -= *= /= %= <<= >>= &= ^= |=`        */ T_OP_14,
  /** `,`                                        */ T_COMMA /* = T_OP_15 */,
  /** End of line: `;`                               */ T_SEMICOLON,
  /** Colon after label: `:`                         */ T_LABEL_COLON,
  /** Backstitch: `@`                                */ T_BACKSTITCH,
  /** Ellipsis: `...`, or non-standard `..`          */ T_ELLIPSIS,
  /** Keyword for deferred resource release          */ T_CONTROL_FLOW_DEFER,
  /** Other token that is not part of the C grammar. */ T_OTHER
} MUT_CONST_TYPE_VARIANTS(TokenType);

static const unsigned char * const
TokenType_STRING[1+T_OTHER] = {
  B("NONE"),
  B("Identifier"),
  B("Type"), B("Type struct"), B("Type qualifier"), B("Type qualifier auto"),
  B("Type definition"),
  B("Control flow conditional"),
  B("Control flow loop"),
  B("Control flow switch"), B("Control flow case"),
  B("Control flow break"), B("Control flow continue"), B("Control flow return"),
  B("Control flow goto"), B("Control flow label"),
  B("Number"), B("String"), B("Character"),
  B("Space"), B("Comment"),
  B("Preprocessor"), B("_Generic keyword"),
  B("Block start"), B("Block end"), B("Tuple start"), B("Tuple end"),
  B("Index start"), B("Index end"),
  B("Group start"), B("Group end"),
  B("Op 1"), B("Op 2"), B("Op 3"), B("Op 4"), B("Op 5"), B("Op 6"),
  B("Op 7"), B("Op 8"), B("Op 9"), B("Op 10"), B("Op 11"), B("Op 12"),
  B("Op 13"), B("Op 14"),
  B("Comma (op 15)"), B("Semicolon"), B("Colon after label"),
  B("Backstitch"),
  B("Ellipsis"),
  B("Defer"),
  B("OTHER")
};

#define precedence(token_type) (token_type - T_OP_1)
#define is_keyword(token_type) (token_type >= T_TYPE and token_type <= T_CONTROL_FLOW_LABEL)
#define is_operator(token_type) (token_type >= T_OP_1 and token_type <= T_COMMA)
#define is_fence(token_type) (token_type >= T_BLOCK_START and token_type <= T_GROUP_END)

/** Marks a C token in the source code. */
typedef struct Marker {
  SrcIndexType start;       /**< Start position, in bytes/chars. */
  SrcLenType   len;         /**< Length, in bytes/chars. */
  mut_TokenType token_type; /**< Token type. */
  bool synthetic;           /**< It does not come directly from parsing. */
} MUT_CONST_TYPE_VARIANTS(Marker);

/** Error while processing markers. */
typedef struct Error {
  Marker_mut_p position; /**< Position at which the problem was noticed. */
  const char * message;  /**< Message for user. */
} MUT_CONST_TYPE_VARIANTS(Error);

#include "array.h"

DEFINE_ARRAY_OF(Marker, 0, {});
DEFINE_ARRAY_OF(TokenType, 0, {});

typedef uint8_t MUT_CONST_TYPE_VARIANTS(Byte);
/** Add 8 bytes after end of buffer to avoid bounds checking while scanning
 * for tokens. No literal token is longer. */
DEFINE_ARRAY_OF(Byte, 8, {});
//DEFINE_ARRAY_OF(Byte_array_slice, 0, {});

/** Append the C string bytes to the end of the given buffer. */
static bool
push_str(mut_Byte_array_p _, const char * const str)
{
  Byte_array_slice insert = { (Byte_p) str, (Byte_p) str + strlen(str) };
  return splice_Byte_array(_, _->len, 0, NULL, insert);
}

/** Append a formatted C string to the end of the given buffer.
 *  It’s similar to `sprintf(...)`, only the result is stored in a byte buffer
 * that grows automatically if needed to hold the complete result. */
__attribute__ ((format (printf, 2, 3))) // GCC, typecheck format string args
static bool
push_fmt(mut_Byte_array_p _, const char * const fmt, ...)
{
  va_list args;
  va_start(args, fmt);

  if (not ensure_capacity_Byte_array(_, _->len + 80)) {
    va_end(args);
    return false;
  }
  size_t available = _->capacity - _->len;
  size_t needed = (size_t)
      vsnprintf((char*) end_of_Byte_array(_), available - 1, fmt, args);
  if (needed > available) {
    if (not ensure_capacity_Byte_array(_, _->len + needed + 1)) {
      va_end(args);
      return false;
    }
    available = _->capacity - _->len;
    vsnprintf((char*) end_of_Byte_array(_), available - 1, fmt, args);
  }
  _->len += (needed > available? available: needed);

  va_end(args);

  return true;
}

/** Make it be a valid C string, by adding a `'\0'` character after the
 * last valid element, without increasing the `.len` value.
 *  It first makes sure there is at least one extra byte after the array
 * contents in memory, by increasing the `.capacity` if needed, and then
 * sets that byte to `0`.
 *  Returns the pointer `_->start` which now can be used as a C string
 * as long as you don’t modify it.
 */
static const char *
as_c_string(mut_Byte_array_p _)
{
  if (_->len is _->capacity and
      not ensure_capacity_Byte_array(_, _->len + 1)) {
    return "OUT OF MEMORY ERROR IN as_c_string()";
  }
  *(_->start + _->len) = 0;
  return (const char *) _->start;
}

typedef FILE* mut_File_p;
typedef char*   mut_FilePath;
typedef const char* FilePath;
/** Read a file into the given buffer. Returns error code, 0 if it succeeds. */
static int
read_file(mut_Byte_array_p _, FilePath path)
{
  int err = 0; // No error.
  mut_File_p input = fopen(path, "r");
  if (not input) return errno;
  fseek(input, 0, SEEK_END);
  size_t size = (size_t)ftell(input);
  if (not ensure_capacity_Byte_array(_, size)) {
    fclose(input);
    return ENOMEM;
  }
  rewind(input);
  _->len = fread(_->start, sizeof(_->start[0]), size, input);
  if (feof(input)) {
    err = EIO;
  } else if (ferror(input)) {
    err = errno;
  } else {
    memset(_->start + _->len, 0,
           (_->capacity - _->len) * sizeof(_->start[0]));
  }
  fclose(input);
  return err;
}

/** Read into the given buffer. Returns error code, 0 if it succeeds. */
static int
read_stream(mut_Byte_array_p _, FILE* input)
{
  int err = 0; // No error.
  const size_t chunk = 4096; // 4 KB, for instance.
  if (not input) return errno;
  while (not feof(input)) {
    if (not ensure_capacity_Byte_array(_, _->len + chunk)) return ENOMEM;
    size_t read = fread(_->start + _->len, sizeof(_->start[0]), chunk, input);
    if (read is 0) break;
    _->len += read;
  }
  if (ferror(input)) {
    err = errno;
  } else {
    memset(_->start + _->len, 0,
           (_->capacity - _->len) * sizeof(_->start[0]));
  }
  return err;
}

/** Print an error produced when reading a file. */
static void
print_file_error(int err, const char* file_name, Byte_array_p src)
{
  switch (err) {
    case EIO:
      eprintln(LANG("Fin de fichero inesperado en %ld leyendo «%s».",
                    "Unexpected EOF at %ld reading “%s”."),
               src->len, file_name);
      break;
    default:
      eprintln(LANG("Error leyendo «%s»: 0x%02X %s.",
                    "Error reading “%s”: 0x%02X %s."),
               file_name, err, strerror(err));
  }
}

/** Initialize a marker with the given values. */
static void
init_Marker(mut_Marker_p _, Byte_p start, Byte_p end, Byte_array_p src,
            TokenType token_type)
{
  _->start      = index_Byte_array(src, start);
  _->len        = (size_t)(end - start);
  _->token_type = token_type;
  _->synthetic  = false;
}

/** Check whether two markers represent the same token. */
static bool
is_same_token(Marker_p a, Marker_p b, Byte_array_p src)
{
  return (a->token_type is b->token_type and
          a->len        is b->len        and
          mem_eq(get_Byte_array(src, a->start),
                 get_Byte_array(src, b->start),
                 a->len)
          );
}

/** Build a new marker for the given string,
 * pointing to its first appearance in `src`.
 *  If not found, append the text to `src`
 * and return a marker poiting there.
 */
static Marker
Marker_from(mut_Byte_array_p src, const char * const text, TokenType token_type)
{
  Byte_mut_p cursor = start_of_Byte_array(src);
  Byte_mut_p match  = NULL;
  Byte_p     end    =   end_of_Byte_array(src);
  size_t text_len = strlen(text);
  Byte_p text_end = (Byte_p)text + text_len;
  const char first_character = text[0];
  while ((cursor = memchr(cursor, first_character, (size_t)(end - cursor)))) {
    Byte_mut_p p1 = cursor;
    Byte_mut_p p2 = (Byte_p) text;
    while (p2 is_not text_end and p1 is_not end and *p1 is *p2) { ++p1; ++p2; }
    if (p2 is text_end) {
      match = cursor;
      break;
    }
    ++cursor;
  }
  mut_Marker marker;
  if (not match) {
    match = end_of_Byte_array(src);
    Byte_array_slice insert = { (Byte_p)text, (Byte_p)text + text_len };
    splice_Byte_array(src, src->len, 0, NULL, insert);
  }
  init_Marker(&marker, match, match + text_len, src, token_type);
  marker.synthetic = true;

  return marker;
}

/* Lexer definitions. */

/** Match an identifier.
 *  Assumes `end` > `start`. */
static inline Byte_p
identifier(Byte_p start, Byte_p end)
{
  Byte_mut_p cursor = start;
  // No need to check bounds thanks to PADDING_Byte_array:
  while (*cursor is '\\' and *(cursor+1) is '\n') {
    // http://www.open-std.org/jtc1/sc22/wg14/www/docs/n897.pdf#18
    // Section 5.1.1.2 paragraph 30, at PDF page 18:
    // A backslash immediately before a newline has long been used to
    // continue string literals, as well as preprocessing command lines.
    // In the interest of easing machine generation of C, and of
    // transporting code to machines with restrictive physical line lengths,
    // the C89 Committee generalized this mechanism to permit any token
    // to be continued by interposing a backslash/newline sequence.
    cursor += 2;
    if (cursor is end) return NULL;
  }
  uint32_t u = 0;
  UTF8Error err = UTF8_NO_ERROR;
  cursor = decode_utf8(cursor, end, &u, &err);
  if (utf8_error(err, (size_t)(cursor - start))) return NULL;
  if (u is '\\') {
    if (cursor is end) return NULL;
    size_t len;
    switch(*cursor) {
      case 'U': len = 8; u = 0; break;
      case 'u': len = 4; u = 0; break;
      default:
        return NULL;
    }
    if (cursor + len >= end) {
      error(LANG("Nombre de carácter universal incompleto.",
                 "Incomplete universal character name."));
      return NULL;
    }
    while (len is_not 0) {
      --len;
      mut_Byte c = *(++cursor);
      Byte value =
          in('0',c,'9')? c-'0':
          in('A',c,'F')? c-'A'+10:
          in('a',c,'f')? c-'a'+10:
          0xFF;
      if (value is 0xFF) {
        error(LANG("Nombre de carácter universal mal formado.",
                   "Malformed universal character name."));
        return NULL;
      }
      u = (u << 4) | value;
    }
  }
  if (u >= 0xD800 and u < 0xE000) {
    error(LANG("Error UTF-8, par subrogado.",
               "UTF-8 error, surrogate pair."));
    return NULL;
  }
  if (in('a',u,'z') or in('A',u,'Z') or u is '_' or
      u > 0x7F and
      (
          // ISO/IEC 9899:TC3 Annex D:
          // Latin:
          u is 0x00AA or u is 0x00BA or
          in(0x00C0,u,0x00D6) or in(0x00D8,u,0x00F6) or in(0x00F8,u,0x01F5) or
          in(0x01FA,u,0x0217) or in(0x0250,u,0x02A8) or in(0x1E00,u,0x1E9B) or
          in(0x1EA0,u,0x1EF9) or u is 0x207F or
          // Greek:
          u is 0x0386 or in(0x0388,u,0x038A) or u is 0x038C or
          in(0x038E,u,0x03A1) or in(0x03A3,u,0x03CE) or in(0x03D0,u,0x03D6) or
          u is 0x03DA or u is 0x03DC or u is 0x03DE or u is 0x03E0 or
          in(0x03E2,u,0x03F3) or in(0x1F00,u,0x1F15) or in(0x1F18,u,0x1F1D) or
          in(0x1F20,u,0x1F45) or in(0x1F48,u,0x1F4D) or in(0x1F50,u,0x1F57) or
          u is 0x1F59 or u is 0x1F5B or u is 0x1F5D or
          in(0x1F5F,u,0x1F7D) or in(0x1F80,u,0x1FB4) or in(0x1FB6,u,0x1FBC) or
          in(0x1FC2,u,0x1FC4) or in(0x1FC6,u,0x1FCC) or in(0x1FD0,u,0x1FD3) or
          in(0x1FD6,u,0x1FDB) or in(0x1FE0,u,0x1FEC) or in(0x1FF2,u,0x1FF4) or
          in(0x1FF6,u,0x1FFC) or
          // Cyrillic:
          in(0x0401,u,0x040C) or in(0x040E,u,0x044F) or in(0x0451,u,0x045C) or
          in(0x045E,u,0x0481) or in(0x0490,u,0x04C4) or in(0x04C7,u,0x04C8) or
          in(0x04CB,u,0x04CC) or in(0x04D0,u,0x04EB) or in(0x04EE,u,0x04F5) or
          in(0x04F8,u,0x04F9) or
          // Armenian:
          in(0x0531,u,0x0556) or in(0x0561,u,0x0587) or
          // Hebrew:
          in(0x05B0,u,0x05B9) or in(0x05F0,u,0x05F2) or
          // Arabic:
          in(0x0621,u,0x063A) or in(0x0640,u,0x0652) or in(0x0670,u,0x06B7) or
          in(0x06BA,u,0x06BE) or in(0x06C0,u,0x06CE) or in(0x06D0,u,0x06DC) or
          in(0x06E5,u,0x06E8) or in(0x06EA,u,0x06ED) or
          // Devanagari:
          in(0x0901,u,0x0903) or in(0x0905,u,0x0939) or in(0x093E,u,0x094D) or
          in(0x0950,u,0x0952) or in(0x0958,u,0x0963) or
          // Bengali:
          in(0x0981,u,0x0983) or in(0x0985,u,0x098C) or in(0x098F,u,0x0990) or
          in(0x0993,u,0x09A8) or in(0x09AA,u,0x09B0) or
          u is 0x09B2 or in(0x09B6,u,0x09B9) or in(0x09BE,u,0x09C4) or
          in(0x09C7,u,0x09C8) or in(0x09CB,u,0x09CD) or in(0x09DC,u,0x09DD) or
          in(0x09DF,u,0x09E3) or in(0x09F0,u,0x09F1) or
          // Gurmukhi:
          u is 0x0A02 or in(0x0A05,u,0x0A0A) or in(0x0A0F,u,0x0A10) or
          in(0x0A13,u,0x0A28) or in(0x0A2A,u,0x0A30) or in(0x0A32,u,0x0A33) or
          in(0x0A35,u,0x0A36) or in(0x0A38,u,0x0A39) or in(0x0A3E,u,0x0A42) or
          in(0x0A47,u,0x0A48) or in(0x0A4B,u,0x0A4D) or in(0x0A59,u,0x0A5C) or
          u is 0x0A5E or u is 0x0A74 or
          // Gujarati:
          in(0x0A81,u,0x0A83) or in(0x0A85,u,0x0A8B) or u is 0x0A8D or
          in(0x0A8F,u,0x0A91) or in(0x0A93,u,0x0AA8) or in(0x0AAA,u,0x0AB0) or
          in(0x0AB2,u,0x0AB3) or in(0x0AB5,u,0x0AB9) or in(0x0ABD,u,0x0AC5) or
          in(0x0AC7,u,0x0AC9) or in(0x0ACB,u,0x0ACD) or
          u is 0x0AD0 or u is 0x0AE0 or
          // Oriya:
          in(0x0B01,u,0x0B03) or in(0x0B05,u,0x0B0C) or in(0x0B0F,u,0x0B10) or
          in(0x0B13,u,0x0B28) or in(0x0B2A,u,0x0B30) or in(0x0B32,u,0x0B33) or
          in(0x0B36,u,0x0B39) or in(0x0B3E,u,0x0B43) or in(0x0B47,u,0x0B48) or
          in(0x0B4B,u,0x0B4D) or in(0x0B5C,u,0x0B5D) or in(0x0B5F,u,0x0B61) or
          // Tamil:
          in(0x0B82,u,0x0B83) or in(0x0B85,u,0x0B8A) or in(0x0B8E,u,0x0B90) or
          in(0x0B92,u,0x0B95) or in(0x0B99,u,0x0B9A) or u is 0x0B9C or
          in(0x0B9E,u,0x0B9F) or in(0x0BA3,u,0x0BA4) or in(0x0BA8,u,0x0BAA) or
          in(0x0BAE,u,0x0BB5) or in(0x0BB7,u,0x0BB9) or in(0x0BBE,u,0x0BC2) or
          in(0x0BC6,u,0x0BC8) or in(0x0BCA,u,0x0BCD) or
          // Telugu:
          in(0x0C01,u,0x0C03) or in(0x0C05,u,0x0C0C) or in(0x0C0E,u,0x0C10) or
          in(0x0C12,u,0x0C28) or in(0x0C2A,u,0x0C33) or in(0x0C35,u,0x0C39) or
          in(0x0C3E,u,0x0C44) or in(0x0C46,u,0x0C48) or in(0x0C4A,u,0x0C4D) or
          in(0x0C60,u,0x0C61) or
          // Kannada:
          in(0x0C82,u,0x0C83) or in(0x0C85,u,0x0C8C) or in(0x0C8E,u,0x0C90) or
          in(0x0C92,u,0x0CA8) or in(0x0CAA,u,0x0CB3) or in(0x0CB5,u,0x0CB9) or
          in(0x0CBE,u,0x0CC4) or in(0x0CC6,u,0x0CC8) or in(0x0CCA,u,0x0CCD) or
          u is 0x0CDE or in(0x0CE0,u,0x0CE1) or
          // Malayalam:
          in(0x0D02,u,0x0D03) or in(0x0D05,u,0x0D0C) or in(0x0D0E,u,0x0D10) or
          in(0x0D12,u,0x0D28) or in(0x0D2A,u,0x0D39) or in(0x0D3E,u,0x0D43) or
          in(0x0D46,u,0x0D48) or in(0x0D4A,u,0x0D4D) or in(0x0D60,u,0x0D61) or
          // Thai:
          in(0x0E01,u,0x0E3A) or in(0x0E40,u,0x0E5B) or
          // Lao:
          in(0x0E81,u,0x0E82) or u is 0x0E84 or in(0x0E87,u,0x0E88) or
          u is 0x0E8A or u is 0x0E8D or
          in(0x0E94,u,0x0E97) or in(0x0E99,u,0x0E9F) or in(0x0EA1,u,0x0EA3) or
          u is 0x0EA5 or u is 0x0EA7 or in(0x0EAA,u,0x0EAB) or
          in(0x0EAD,u,0x0EAE) or in(0x0EB0,u,0x0EB9) or in(0x0EBB,u,0x0EBD) or
          in(0x0EC0,u,0x0EC4) or u is 0x0EC6 or
          in(0x0EC8,u,0x0ECD) or in(0x0EDC,u,0x0EDD) or
          // Tibetan:
          u is 0x0F00 or in(0x0F18,u,0x0F19) or u is 0x0F35 or u is 0x0F37 or
          u is 0x0F39 or in(0x0F3E,u,0x0F47) or in(0x0F49,u,0x0F69) or
          in(0x0F71,u,0x0F84) or in(0x0F86,u,0x0F8B) or in(0x0F90,u,0x0F95) or
          u is 0x0F97 or in(0x0F99,u,0x0FAD) or in(0x0FB1,u,0x0FB7) or
          u is 0x0FB9 or
          // Georgian:
          in(0x10A0,u,0x10C5) or in(0x10D0,u,0x10F6) or
          // Hiragana:
          in(0x3041,u,0x3093) or in(0x309B,u,0x309C) or
          // Katakana:
          in(0x30A1,u,0x30F6) or in(0x30FB,u,0x30FC) or
          // Bopomofo:
          in(0x3105,u,0x312C) or
          // CJK Unified Ideographs:
          in(0x4E00,u,0x9FA5) or
          // Hangul:
          in(0xAC00,u,0xD7A3) or
          // Digits:
          /* “Note that a strictly conforming program [...]
             may not begin an identifier with an extended digit.”
             http://www.open-std.org/jtc1/sc22/wg14/www/C99RationaleV5.10.pdf#58
          in(0x0660,u,0x0669) or in(0x06F0,u,0x06F9) or in(0x0966,u,0x096F) or
          in(0x09E6,u,0x09EF) or in(0x0A66,u,0x0A6F) or in(0x0AE6,u,0x0AEF) or
          in(0x0B66,u,0x0B6F) or in(0x0BE7,u,0x0BEF) or in(0x0C66,u,0x0C6F) or
          in(0x0CE6,u,0x0CEF) or in(0x0D66,u,0x0D6F) or in(0x0E50,u,0x0E59) or
          in(0x0ED0,u,0x0ED9) or in(0x0F20,u,0x0F33) or
          */
          // Special characters:
          u is 0x00B5 or u is 0x00B7 or in(0x02B0,u,0x02B8) or u is 0x02BB or
          in(0x02BD,u,0x02C1) or in(0x02D0,u,0x02D1) or in(0x02E0,u,0x02E4) or
          u is 0x037A or u is 0x0559 or u is 0x093D or u is 0x0B3D or
          u is 0x1FBE or in(0x203F,u,0x2040) or u is 0x2102 or u is 0x2107 or
          in(0x210A,u,0x2113) or u is 0x2115 or in(0x2118,u,0x211D) or
          u is 0x2124 or u is 0x2126 or u is 0x2128 or in(0x212A,u,0x2131) or
          in(0x2133,u,0x2138) or in(0x2160,u,0x2182) or in(0x3005,u,0x3007) or
          in(0x3021,u,0x3029)
       )) {
    while (cursor < end) {
      // Use `p` here because we need to return `cursor`
      // if `*p` is no longer part of the identifier.
      Byte_mut_p p = decode_utf8(cursor, end, &u, &err);
      if (utf8_error(err, (size_t)(p - start))) return NULL;
      if (u is '\\') {
        if (p is end) {
          // `cursor` can not be `start` if we reached this point.
          return cursor;
        }
        if (*p is '\n') {
          // See above about n897.pdf “C9X Rationale”.
          cursor = p + 1;
          continue;
        } else {
          size_t len;
          switch(*p) {
            case 'U': len = 8; u = 0; break;
            case 'u': len = 4; u = 0; break;
            default:
              --p;
              return p is start? NULL: p;
          }
          if (p + len >= end) {
            error("Incomplete universal character name.");
            return NULL;
          }
          while (len is_not 0) {
            --len;
            mut_Byte c = *(++p);
            Byte value =
                in('0',c,'9')? c-'0':
                in('A',c,'F')? c-'A':
                in('a',c,'F')? c-'a':
                0xFF;
            if (value is 0xFF) {
              error("Malformed universal character name.");
              return NULL;
            }
            u = (u << 4) | value;
          }
        }
      }
      if (u >= 0xD800 and u < 0xE000) {
        error(LANG("Error UTF-8, par subrogado.",
                   "UTF-8 error, surrogate pair."));
        return NULL;
      }
      if (in('a',u,'z') or in('A',u,'Z') or u is '_' or in('0',u,'9')) {
        cursor = p;
      } else if (
          u > 0x7F and
          (
          // ISO/IEC 9899:TC3 Annex D:
          // Latin:
          u is 0x00AA or u is 0x00BA or
          in(0x00C0,u,0x00D6) or in(0x00D8,u,0x00F6) or in(0x00F8,u,0x01F5) or
          in(0x01FA,u,0x0217) or in(0x0250,u,0x02A8) or in(0x1E00,u,0x1E9B) or
          in(0x1EA0,u,0x1EF9) or u is 0x207F or
          // Greek:
          u is 0x0386 or in(0x0388,u,0x038A) or u is 0x038C or
          in(0x038E,u,0x03A1) or in(0x03A3,u,0x03CE) or in(0x03D0,u,0x03D6) or
          u is 0x03DA or u is 0x03DC or u is 0x03DE or u is 0x03E0 or
          in(0x03E2,u,0x03F3) or in(0x1F00,u,0x1F15) or in(0x1F18,u,0x1F1D) or
          in(0x1F20,u,0x1F45) or in(0x1F48,u,0x1F4D) or in(0x1F50,u,0x1F57) or
          u is 0x1F59 or u is 0x1F5B or u is 0x1F5D or
          in(0x1F5F,u,0x1F7D) or in(0x1F80,u,0x1FB4) or in(0x1FB6,u,0x1FBC) or
          in(0x1FC2,u,0x1FC4) or in(0x1FC6,u,0x1FCC) or in(0x1FD0,u,0x1FD3) or
          in(0x1FD6,u,0x1FDB) or in(0x1FE0,u,0x1FEC) or in(0x1FF2,u,0x1FF4) or
          in(0x1FF6,u,0x1FFC) or
          // Cyrillic:
          in(0x0401,u,0x040C) or in(0x040E,u,0x044F) or in(0x0451,u,0x045C) or
          in(0x045E,u,0x0481) or in(0x0490,u,0x04C4) or in(0x04C7,u,0x04C8) or
          in(0x04CB,u,0x04CC) or in(0x04D0,u,0x04EB) or in(0x04EE,u,0x04F5) or
          in(0x04F8,u,0x04F9) or
          // Armenian:
          in(0x0531,u,0x0556) or in(0x0561,u,0x0587) or
          // Hebrew:
          in(0x05B0,u,0x05B9) or in(0x05F0,u,0x05F2) or
          // Arabic:
          in(0x0621,u,0x063A) or in(0x0640,u,0x0652) or in(0x0670,u,0x06B7) or
          in(0x06BA,u,0x06BE) or in(0x06C0,u,0x06CE) or in(0x06D0,u,0x06DC) or
          in(0x06E5,u,0x06E8) or in(0x06EA,u,0x06ED) or
          // Devanagari:
          in(0x0901,u,0x0903) or in(0x0905,u,0x0939) or in(0x093E,u,0x094D) or
          in(0x0950,u,0x0952) or in(0x0958,u,0x0963) or
          // Bengali:
          in(0x0981,u,0x0983) or in(0x0985,u,0x098C) or in(0x098F,u,0x0990) or
          in(0x0993,u,0x09A8) or in(0x09AA,u,0x09B0) or
          u is 0x09B2 or in(0x09B6,u,0x09B9) or in(0x09BE,u,0x09C4) or
          in(0x09C7,u,0x09C8) or in(0x09CB,u,0x09CD) or in(0x09DC,u,0x09DD) or
          in(0x09DF,u,0x09E3) or in(0x09F0,u,0x09F1) or
          // Gurmukhi:
          u is 0x0A02 or in(0x0A05,u,0x0A0A) or in(0x0A0F,u,0x0A10) or
          in(0x0A13,u,0x0A28) or in(0x0A2A,u,0x0A30) or in(0x0A32,u,0x0A33) or
          in(0x0A35,u,0x0A36) or in(0x0A38,u,0x0A39) or in(0x0A3E,u,0x0A42) or
          in(0x0A47,u,0x0A48) or in(0x0A4B,u,0x0A4D) or in(0x0A59,u,0x0A5C) or
          u is 0x0A5E or u is 0x0A74 or
          // Gujarati:
          in(0x0A81,u,0x0A83) or in(0x0A85,u,0x0A8B) or u is 0x0A8D or
          in(0x0A8F,u,0x0A91) or in(0x0A93,u,0x0AA8) or in(0x0AAA,u,0x0AB0) or
          in(0x0AB2,u,0x0AB3) or in(0x0AB5,u,0x0AB9) or in(0x0ABD,u,0x0AC5) or
          in(0x0AC7,u,0x0AC9) or in(0x0ACB,u,0x0ACD) or
          u is 0x0AD0 or u is 0x0AE0 or
          // Oriya:
          in(0x0B01,u,0x0B03) or in(0x0B05,u,0x0B0C) or in(0x0B0F,u,0x0B10) or
          in(0x0B13,u,0x0B28) or in(0x0B2A,u,0x0B30) or in(0x0B32,u,0x0B33) or
          in(0x0B36,u,0x0B39) or in(0x0B3E,u,0x0B43) or in(0x0B47,u,0x0B48) or
          in(0x0B4B,u,0x0B4D) or in(0x0B5C,u,0x0B5D) or in(0x0B5F,u,0x0B61) or
          // Tamil:
          in(0x0B82,u,0x0B83) or in(0x0B85,u,0x0B8A) or in(0x0B8E,u,0x0B90) or
          in(0x0B92,u,0x0B95) or in(0x0B99,u,0x0B9A) or u is 0x0B9C or
          in(0x0B9E,u,0x0B9F) or in(0x0BA3,u,0x0BA4) or in(0x0BA8,u,0x0BAA) or
          in(0x0BAE,u,0x0BB5) or in(0x0BB7,u,0x0BB9) or in(0x0BBE,u,0x0BC2) or
          in(0x0BC6,u,0x0BC8) or in(0x0BCA,u,0x0BCD) or
          // Telugu:
          in(0x0C01,u,0x0C03) or in(0x0C05,u,0x0C0C) or in(0x0C0E,u,0x0C10) or
          in(0x0C12,u,0x0C28) or in(0x0C2A,u,0x0C33) or in(0x0C35,u,0x0C39) or
          in(0x0C3E,u,0x0C44) or in(0x0C46,u,0x0C48) or in(0x0C4A,u,0x0C4D) or
          in(0x0C60,u,0x0C61) or
          // Kannada:
          in(0x0C82,u,0x0C83) or in(0x0C85,u,0x0C8C) or in(0x0C8E,u,0x0C90) or
          in(0x0C92,u,0x0CA8) or in(0x0CAA,u,0x0CB3) or in(0x0CB5,u,0x0CB9) or
          in(0x0CBE,u,0x0CC4) or in(0x0CC6,u,0x0CC8) or in(0x0CCA,u,0x0CCD) or
          u is 0x0CDE or in(0x0CE0,u,0x0CE1) or
          // Malayalam:
          in(0x0D02,u,0x0D03) or in(0x0D05,u,0x0D0C) or in(0x0D0E,u,0x0D10) or
          in(0x0D12,u,0x0D28) or in(0x0D2A,u,0x0D39) or in(0x0D3E,u,0x0D43) or
          in(0x0D46,u,0x0D48) or in(0x0D4A,u,0x0D4D) or in(0x0D60,u,0x0D61) or
          // Thai:
          in(0x0E01,u,0x0E3A) or in(0x0E40,u,0x0E5B) or
          // Lao:
          in(0x0E81,u,0x0E82) or u is 0x0E84 or in(0x0E87,u,0x0E88) or
          u is 0x0E8A or u is 0x0E8D or
          in(0x0E94,u,0x0E97) or in(0x0E99,u,0x0E9F) or in(0x0EA1,u,0x0EA3) or
          u is 0x0EA5 or u is 0x0EA7 or in(0x0EAA,u,0x0EAB) or
          in(0x0EAD,u,0x0EAE) or in(0x0EB0,u,0x0EB9) or in(0x0EBB,u,0x0EBD) or
          in(0x0EC0,u,0x0EC4) or u is 0x0EC6 or
          in(0x0EC8,u,0x0ECD) or in(0x0EDC,u,0x0EDD) or
          // Tibetan:
          u is 0x0F00 or in(0x0F18,u,0x0F19) or u is 0x0F35 or u is 0x0F37 or
          u is 0x0F39 or in(0x0F3E,u,0x0F47) or in(0x0F49,u,0x0F69) or
          in(0x0F71,u,0x0F84) or in(0x0F86,u,0x0F8B) or in(0x0F90,u,0x0F95) or
          u is 0x0F97 or in(0x0F99,u,0x0FAD) or in(0x0FB1,u,0x0FB7) or
          u is 0x0FB9 or
          // Georgian:
          in(0x10A0,u,0x10C5) or in(0x10D0,u,0x10F6) or
          // Hiragana:
          in(0x3041,u,0x3093) or in(0x309B,u,0x309C) or
          // Katakana:
          in(0x30A1,u,0x30F6) or in(0x30FB,u,0x30FC) or
          // Bopomofo:
          in(0x3105,u,0x312C) or
          // CJK Unified Ideographs:
          in(0x4E00,u,0x9FA5) or
          // Hangul:
          in(0xAC00,u,0xD7A3) or
          // Digits:
          in(0x0660,u,0x0669) or in(0x06F0,u,0x06F9) or in(0x0966,u,0x096F) or
          in(0x09E6,u,0x09EF) or in(0x0A66,u,0x0A6F) or in(0x0AE6,u,0x0AEF) or
          in(0x0B66,u,0x0B6F) or in(0x0BE7,u,0x0BEF) or in(0x0C66,u,0x0C6F) or
          in(0x0CE6,u,0x0CEF) or in(0x0D66,u,0x0D6F) or in(0x0E50,u,0x0E59) or
          in(0x0ED0,u,0x0ED9) or in(0x0F20,u,0x0F33) or
          // Special characters:
          u is 0x00B5 or u is 0x00B7 or in(0x02B0,u,0x02B8) or u is 0x02BB or
          in(0x02BD,u,0x02C1) or in(0x02D0,u,0x02D1) or in(0x02E0,u,0x02E4) or
          u is 0x037A or u is 0x0559 or u is 0x093D or u is 0x0B3D or
          u is 0x1FBE or in(0x203F,u,0x2040) or u is 0x2102 or u is 0x2107 or
          in(0x210A,u,0x2113) or u is 0x2115 or in(0x2118,u,0x211D) or
          u is 0x2124 or u is 0x2126 or u is 0x2128 or in(0x212A,u,0x2131) or
          in(0x2133,u,0x2138) or in(0x2160,u,0x2182) or in(0x3005,u,0x3007) or
          in(0x3021,u,0x3029)
           )) {
        cursor = p;
      } else {
        break;
      }
    }
    return cursor;
  } else {
    return NULL;
  }
}

/** Match a number.
 *  This matches invalid numbers like 3.4.6, 09, and 3e23.48.34e+11.
 *  See ISO/IEC 9899:TC3 6.4.8 “Preprocessing numbers”.
 *  Rejecting that is left to the compiler.
 *  Assumes `end` > `start`. */
static inline Byte_p
number(Byte_p start, Byte_p end)
{
  Byte_mut_p cursor = start;
  mut_Byte c = *cursor;
  if (c is '.') { ++cursor; if (cursor is end) return NULL; c = *cursor; }
  // No need to check bounds thanks to PADDING_Byte_array:
  if (c is '\\' and *(cursor+1) is '\n') { cursor += 2; c = *cursor; }
  if (in('0',c,'9')) {
    ++cursor;
    while (cursor is_not end) {
      c = *cursor;
      if (in('0',c,'9')) {
        ++cursor;
        continue;
      } else if (c is '.') {
        if (*(cursor - 1) is '.') return cursor - 1;
        ++cursor;
        continue;
      } else if (c is 'e' or c is 'E' or c is 'p' or c is 'P') {
        if (++cursor is end) break;
        c = *cursor;
        if (c is '+' or c is '-') {
          ++cursor;
          continue;
        }
      } else if (c is '\\' and *(cursor+1) is '\n') {
        // “C8X Rationale” n897.pdf 5.1.1.2 paragraph 30
        // http://www.open-std.org/jtc1/sc22/wg14/www/docs/n897.pdf#18
        cursor += 2;
        continue;
      }
      Byte_p identifier_nondigit = identifier(cursor, end);
      if (not identifier_nondigit) break;
      cursor = identifier_nondigit;
    }
    return cursor;
  } else {
    return NULL;
  }
}

/** Match a string literal.
 *  Assumes `end` > `start`. */
static inline Byte_p
string(Byte_p start, Byte_p end)
{
  if (*start is_not '"') return NULL;
  Byte_mut_p cursor = start;
  while ((cursor = memchr(cursor + 1, '"', (size_t)(end - (cursor + 1))))) {
    Byte_mut_p p = cursor;
    while (*--p is '\\');
    if ((p - cursor) & 1) {
      return cursor + 1; // End is past the closing symbol.
    }
  }
  error(LANG("Cadena literal interrumpida.",
             "Unterminated string literal."));
  return NULL;
}

/** Match a character literal.
 *  Assumes `end` > `start`. */
static inline Byte_p
character(Byte_p start, Byte_p end)
{
  if (*start is_not '\'') return NULL;
  Byte_mut_p cursor = start;
  while ((cursor = memchr(cursor + 1, '\'', (size_t)(end - (cursor + 1))))) {
    Byte_mut_p p = cursor;
    while (*--p is '\\');
    if ((p - cursor) & 1) {
      return cursor + 1; // End is past the closing symbol.
    }
  }
  error(LANG("Carácter literal interrumpido.",
             "Unterminated character literal."));
  return NULL;
}

/** Match whitespace: one or more space, `TAB`, `CR`, or `NL` characters.
 *  Assumes `end` > `start`. */
static inline Byte_p
space(Byte_p start, Byte_p end)
{
  Byte_mut_p cursor = start;
  while (cursor < end) {
    switch (*cursor) {
      case ' ': case '\t': case '\n': case '\r':
        ++cursor;
        break;
      case '\\':
        // No need to check bounds thanks to PADDING_Byte_array:
        if (*(cursor + 1) is_not '\n') goto exit;
        ++cursor;
        break;
      default:
        goto exit;
        // Unreachable: break;
    }
  } exit:
  return (cursor is start)? NULL: cursor;
}

/** Match a comment block.
 *  Assumes `end` > `start`. */
static inline Byte_p
comment(Byte_p start, Byte_p end)
{
  if (*start is_not '/') return NULL;
  Byte_mut_p cursor = start + 1;
  if (*cursor is '/') {
    do {
      cursor = memchr(cursor + 1, '\n', (size_t)(end - cursor));
      if (not cursor) { cursor = end; break; }
    } while ('\\' is *(cursor - 1));
    return cursor; // The newline is not part of the token.
  } else if (*cursor is_not '*') {
    return NULL;
  }
  ++cursor; // Skip next character, at minimum a '*' if the comment is empty.
  if (cursor is end) return NULL;
  do {
    cursor = memchr(cursor + 1, '/', (size_t)(end - cursor));
    if (not cursor) { cursor = end; break; }
  } while (*(cursor - 1) is_not '*');
  return (cursor is end)? end: cursor + 1;// Token includes the closing symbol.
}

/** Match a pre-processor directive.
 *  Assumes `end` > `start`. */
static inline Byte_p
preprocessor(Byte_p start, Byte_p end)
{
  if (start is end or *start is_not '#') return NULL;
  Byte_mut_p cursor = start + 1;
  if (cursor is_not end and *cursor is '#') return cursor + 1;
  // #ident/#sccs is not a standard directive:
  // https://gcc.gnu.org/onlinedocs/cpp/Other-Directives.html#Other-Directives
  //
  // #include_next is a GCC extension:
  // https://gcc.gnu.org/onlinedocs/cpp/Wrapper-Headers.html#Wrapper-Headers
  //
  // #assert is incompatible with checking in #foreach that an argument name
  // after # is defined, and this check is more important.
  // https://gcc.gnu.org/onlinedocs/cpp/Obsolete-Features.html#Obsolete-Features
  if        (                        cursor + 12 <= end and
             (mem_eq("include_next", cursor,  12))) {
    cursor =                         cursor + 12;
  } else if (                   cursor + 7 <= end and
             (mem_eq("include", cursor,  7) or mem_eq("warning", cursor, 7) or
              mem_eq("foreach", cursor,  7))) {
    cursor =                    cursor + 7;
  } else if (                  cursor + 6 <= end and
             (mem_eq("define", cursor,  6) or mem_eq("pragma", cursor, 6) or
              mem_eq("ifndef", cursor,  6) or mem_eq("import", cursor, 6))) {
    cursor =                   cursor + 6;
  } else if (                 cursor + 5 <= end and
             (mem_eq("endif", cursor,  5) or mem_eq("error", cursor, 5) or
              mem_eq("ifdef", cursor,  5) or mem_eq("undef", cursor, 5) or
              mem_eq("ident", cursor,  5))) {
    cursor =                  cursor + 5;
  } else if (                cursor + 4 <= end and
             (mem_eq("line", cursor,  4) or mem_eq("sccs", cursor, 4) or
              mem_eq("ifeq", cursor,  4) or mem_eq("elif", cursor, 4) or
              mem_eq("else", cursor,  4))) {
    cursor =                 cursor + 4;
  } else if (               cursor + 2 <= end and mem_eq("if", cursor, 2)) {
    cursor =                cursor + 2;
  } else {
    // https://gcc.gnu.org/onlinedocs/cpp/Stringizing.html#Stringizing
    // Single #, may be expanded if inside a #foreach block.
    return cursor;
  }
  if (cursor is end) return end;
  if (*cursor is ' ' or *cursor is '\n') {
    do {
      if (cursor is end) break;
      cursor = memchr(cursor + 1, '\n', (size_t)(end - cursor));
      if (not cursor) { cursor = end; break; }
    } while ('\\' is *(cursor - 1));
  }
  return cursor; // The newline is not part of the token.
}

/** Fallback match, just read one UTF-8 Unicode® Code Point
 * as one token of type `T_OTHER`.
 *  Assumes `end` > `start`. */
static inline Byte_p
other(Byte_p start, Byte_p end)
{
  mut_Byte c = *start;
  if      (0x00 is (c & 0x80)) { return start + 1; }
  else if (0xC0 is (c & 0xE0)) { return start + 2; }
  else if (0xE0 is (c & 0xF0)) { return start + 3; }
  else if (0xF0 is (c & 0xF8)) { return start + 4; }
  else                           return NULL;
}


/** Get new slice for the given marker. */
static Byte_array_slice
slice_for_marker(Byte_array_p src, Marker_p cursor)
{
  Byte_array_slice slice = {
    // get_mut_Byte_array() is not valid at the end.
    .start_p = get_Byte_array(src, cursor->start),
    .end_p   = get_Byte_array(src, cursor->start) + cursor->len
  };
  return slice;
}

/** Copy the characters between `start` and `end` into the given Byte array,
 * appending them to the end of that Byte array.
 *  If you want to replace any previous content,
 * do `string.len = 0;` before calling this function.
 *
 *  To extract the text for `Marker_p m` from `Byte_p src`:
 * ```
 * mut_Byte_array string = init_Byte_array(20);
 * extract_src(m, m + 1, src, &string);
 * destruct_Byte_array(&string);
 * ```
 *  If you want string.start to be a zero-terminated C string:
 * ```
 * push_Byte_array(&string, '\0');
 * ```
 *  or use `as_c_string(mut_Byte_array_p _)`.
 *
 * @param[in] start marker pointer.
 * @param[in] end marker pointer.
 * @param[in] src byte buffer with the source code.
 * @param[out] string Byte buffer to receive the bytes copied from the segment.
 */
static void
extract_src(Marker_p start, Marker_p end, Byte_array_p src, mut_Byte_array_p string)
{
  Marker_mut_p cursor = start;
  while (cursor < end) {
    splice_Byte_array(string, string->len, 0, NULL,
                      slice_for_marker(src, cursor));
    ++cursor;
  }
}

/** Check whether the text in a marker is the same as the given string. */
static inline bool
src_eq(Marker_p marker, Byte_array_p string, Byte_array_p src)
{
  if (marker->len is_not string->len) return false;
  return mem_eq(get_Byte_array(src, marker->start), string->start, string->len);
}

/** Count appearances of the given byte in a marker. */
static inline size_t
count_appearances(Byte byte, Marker_p start, Marker_p end, Byte_array_p src)
{
  size_t count = 0;
  Marker_mut_p cursor = end;
  while (cursor is_not start) {
    --cursor;
    Byte_array_slice src_segment = slice_for_marker(src, cursor);
    Byte_mut_p pointer = src_segment.start_p;
    while ((pointer =
            memchr(pointer, byte, (size_t)(src_segment.end_p - pointer) ))) {
      ++pointer;
      ++count;
    }
  }
  return count;
}

/** Check whether a given byte appears in a marker.
 *  It is faster than `count_appearances(byte, marker, marker+1, src) != 0`.
 */
static inline bool
has_byte(Byte byte, Marker_p marker, Byte_array_p src)
{
  Byte_array_slice src_segment = slice_for_marker(src, marker);
  Byte_mut_p pointer = src_segment.start_p;
  return !!memchr(pointer, byte, (size_t)(src_segment.end_p - pointer));
}

/** Skip forward all `T_SPACE` and `T_COMMENT` markers. */
static inline Marker_p
skip_space_forward(Marker_mut_p start, Marker_p end) {
  while (start is_not end and
         (start->token_type is T_SPACE or start->token_type is T_COMMENT)
         ) ++start;
  return start;
}

/** Skip backward all `T_SPACE` and `T_COMMENT` markers. */
static inline Marker_p
skip_space_back(Marker_p start, Marker_mut_p end) {
  while (end is_not start and
         ((end-1)->token_type is T_SPACE or (end-1)->token_type is T_COMMENT)
         ) --end;
  return end;
}

/** Find matching fence starting at `cursor`, which should point to an
 * opening fence `{`, `[` or `(`, advance until the corresponding
 * closing fence `}`, `]` or `)` is found, then return that address.
 *  If the fences are not closed, the return value is `end`
 * and an error message is stored in `err`.
 */
static inline Marker_p
find_matching_fence(Marker_p cursor, Marker_p end, mut_Error_p err)
{
  // TODO: search backwards if we start at a closing fence.
  Marker_mut_p matching_close = cursor;
  size_t nesting = 0;
  do {
    switch (matching_close->token_type) {
      case T_BLOCK_START: case T_TUPLE_START: case T_INDEX_START:
        ++nesting;
        break;
      case T_BLOCK_END: case T_TUPLE_END: case T_INDEX_END:
        --nesting;
        break;
      default: break;
    }
    ++matching_close;
  } while (matching_close is_not end and nesting);

  if (nesting or matching_close is end) {
    err->message = LANG("Grupo sin cerrar.", "Unclosed group.");
    err->position = cursor;
  }

  return matching_close;
}

/** Find start of line that contains `cursor`,
 * looking back no further than `start`.
 */
static inline Marker_p
find_line_start(Marker_p cursor, Marker_p start, mut_Error_p err)
{
  Marker_mut_p start_of_line = cursor + 1;
  size_t nesting = 0;
  while (start_of_line is_not start) {
    --start_of_line;
    switch (start_of_line->token_type) {
      case T_SEMICOLON: case T_LABEL_COLON:
      case T_BLOCK_START: case T_BLOCK_END:
      case T_PREPROCESSOR:
        if (not nesting and start_of_line is_not cursor) {
          ++start_of_line;
          goto found;
        }
        break;
      case T_TUPLE_START: case T_INDEX_START:
        if (not nesting) {
          ++start_of_line;
          goto found;
        } else {
          --nesting;
        }
        break;
      case T_TUPLE_END: case T_INDEX_END:
        ++nesting;
        break;
      default:
        break;
    }
  } found:

  if (nesting or start_of_line < start) {
    err->message = LANG("Demasiados cierres de grupo.",
                        "Excess group closings.");
    err->position = cursor;
  }

  return start_of_line;
}

/** Find end of line that contains `cursor`,
 * looking forward no further than right before `end`.
 */
static inline Marker_p
find_line_end(Marker_p cursor, Marker_p end, mut_Error_p err)
{
  Marker_mut_p end_of_line = cursor;
  size_t nesting = 0;
  while (end_of_line is_not end) {
    switch (end_of_line->token_type) {
      case T_SEMICOLON: case T_LABEL_COLON: case T_BACKSTITCH:
        if (not nesting) goto found;
        break;
      case T_BLOCK_START: case T_TUPLE_START: case T_INDEX_START:
        ++nesting;
        break;
      case T_BLOCK_END: case T_TUPLE_END: case T_INDEX_END:
        if (not nesting) goto found;
        else              --nesting;
        break;
      default: break;
    }
    ++end_of_line;
  } found:

  if (nesting or end_of_line is end) {
    err->message = LANG("Grupo sin cerrar.", "Unclosed group.");
    err->position = cursor;
  }

  return end_of_line;
}

/** Find start of block that contains `cursor`,
 * looking back no further than `start`.
 */
static inline Marker_p
find_block_start(Marker_p cursor, Marker_p start, mut_Error_p err)
{
  Marker_mut_p start_of_block = cursor + 1;
  size_t nesting = 0;
  while (start_of_block >= start) {
    --start_of_block;
    switch (start_of_block->token_type) {
      case T_BLOCK_START:
        if (not nesting) {
          ++start_of_block;
          goto found;
        } else {
          --nesting;
        }
        break;
      case T_BLOCK_END:
        ++nesting;
        break;
      default: break;
    }
  } found:

  if (nesting or start_of_block < start) {
    err->message = LANG("Demasiados cierres de bloque.",
                        "Excess block closings.");
    err->position = cursor;
  }

  return start_of_block;
}

/** Find end of block that contains `cursor`,
 * looking forward no further than right before `end`.
 */
static inline Marker_p
find_block_end(Marker_p cursor, Marker_p end, mut_Error_p err)
{
  Marker_mut_p end_of_block = cursor;
  size_t nesting = 0;
  while (end_of_block is_not end) {
    switch (end_of_block->token_type) {
      case T_BLOCK_START:
        ++nesting;
        break;
      case T_BLOCK_END:
        if (not nesting) goto found;
        --nesting;
        break;
      default: break;
    }
    ++end_of_block;
  } found:

  if (nesting or end_of_block is end) {
    err->message = LANG("Bloque sin cerrar.", "Unclosed block.");
    err->position = cursor;
  }

  return end_of_block;
}

/** Extract the indentation of the line for the marker at `cursor`,
 * including the preceding `LF` character if it exists.
 * @param[in] markers array of markers.
 * @param[in] cursor position, must not be NULL.
 * @param[in] already_at_line_start whether the cursor is already at the
 *            start of the line, to avoid searching for it if not needed.
 * @param[in] src source code.
 * If there is no indentation, that is if `cursor` is at the start of the file,
 * returns `(Marker){ 0, 0, T_NONE }`.
 */
static Marker
indentation(Marker_array_p markers, Marker_mut_p cursor,
            bool already_at_line_start,
            Byte_array_p src)
{
  assert(cursor is_not NULL);
  mut_Marker indentation = { 0, 0, T_NONE }; // Same as {0}.
  Marker_p start = start_of_Marker_array(markers);
  mut_Error err = {0};
  if (not already_at_line_start) {
    cursor = find_line_start(cursor, start, &err);
    if (err.message) {
      eprintln(LANG("Error: %s",
                    "Error: %s"),
               err.message);
      return indentation;
    }
  }
  if (cursor->token_type is_not T_SPACE) {
    // This happens if the cursor was already at the beginning of the file.
    return indentation;
  }
  indentation = *cursor;
  // Prefer space tokens with LF if available, for the case where we have
  // a comment at the end of the previous line.
  Marker_p end = end_of_Marker_array(markers);
  while (cursor is_not end and
         (cursor->token_type is T_SPACE or cursor->token_type is T_COMMENT)
         ) {
    ++cursor;
    if (cursor->token_type is T_SPACE and has_byte('\n', cursor, src)) {
      indentation = *cursor;
    } else if (cursor->token_type is_not T_COMMENT) {
      break;
    }
  }
  // Remove empty lines, and trailing space in previous line.
  Byte_array_slice slice = slice_for_marker(src, &indentation);
  Byte_mut_p b = slice.end_p;
  while (b is_not slice.start_p) {
    if ('\n' is *(--b)) {
      indentation.start = (size_t)(b - start_of_Byte_array(src));
      indentation.len   = (size_t)(slice.end_p - b);
      break;
    }
  }

  return indentation;
}

/** Compute the line number in the current state of the file. */
static size_t
line_number(Byte_array_p src, Marker_array_p markers, Marker_p position)
{
  assert(position >= markers->start and
         position <= markers->start + markers->len);
  return 1 + count_appearances('\n', markers->start, position, src);
}

/** Compute the line number in the original file. */
static size_t
original_line_number(size_t position, Byte_array_p src)
{
  assert(position <= src->len);
  size_t line = 1;
  Byte_p end = get_Byte_array(src, position);
  Byte_mut_p cursor = start_of_Byte_array(src);
  while (cursor < end) {
    cursor = memchr(cursor, '\n', (size_t)(end - cursor));
    if (not cursor) break;
    ++cursor;
    ++line;
  }
  return line;
}

/** Truncate the markers at the given position
 * and append a pre-processor error directive.
 * `src` is needed to create the new marker for `message`,
 * and `message` can be discarded right after calling this function.
 */
static void
error_at(const char * message, Marker_p cursor, mut_Marker_array_p _, mut_Byte_array_p src)
{
  assert(cursor >= _->start and cursor <= _->start + _->len);
  _->len = index_Marker_array(_, cursor);
  mut_Byte_array buffer = init_Byte_array(200);
  bool ok =
      push_fmt(&buffer, "\n#line %lu", original_line_number(cursor->start,
                                                            src)) &&
      push_str(&buffer, "\n#error ")                              &&
      push_str(&buffer, message)                                  &&
      push_str(&buffer, "\n")                                     &&
      push_Marker_array(_, Marker_from(src, as_c_string(&buffer),
                                       T_PREPROCESSOR));
  if (not ok) error("OUT OF MEMORY ERROR.");
  destruct_Byte_array(&buffer);
}

/** Similar to error_at() but instead of modifying the marker array
 * it writes the message immediately to the output stream.
 * This one is meant for the `unparse*()` functions.
 */
static void
write_error_at(const char * message,
               Marker_p cursor, Byte_array_p src,
               FILE* out)
{
  fprintf(out, "\n#line %lu", original_line_number(cursor->start, src));
  fprintf(out, "\n#error %s\n", message);
}

/** ISO/IEC 9899:TC3 WG14/N1256 §6.7.8 page 126:
 * “14 An array of character type may be initialized by a character string
 *     literal, optionally enclosed in braces. Successive characters of the
 *     character string literal (including the terminating null character
 *     if there is room or if the array is of unknown size) initialize the
 *     elements of the array.”
 * http://www.open-std.org/jtc1/sc22/WG14/www/docs/n1256.pdf#138
 */
static const char spacing[80] =
    //123456789/123456789/123456789/123456789
    "                                        "
    "                                        ";
/** Tabulator position when printing markers in `print_markers()`. */
static const size_t markers_tabulator = 20;
/** Right margin position for the newline escapes in block macros. */
static const size_t right_margin = sizeof(spacing) - 2;

/** Indent by the given number of spaces, for the next `eprint()`
 * or `fprintf(stder, …)`. */
static void
indent_eprint(size_t indent)
{
  if (indent > sizeof(spacing)) indent = sizeof(spacing);
  while (indent-- is_not 0) eprint(" ");
}
/** Tabulate to the given number of spaces, for the next `eprint()`
 * or `fprintf(stder, …)`. */
static void
tabulate_eprint(size_t skip, size_t tabulator)
{
  if (skip > tabulator) skip = tabulator;
  while (skip++ is_not tabulator) eprint(" ");
}

/** Print a human-legible dump of the given marker to stderr.
 * If you want to print several consecutive markers,
 * `print_markers()` is much more efficient because this function
 * requires one heap allocation for each call, which is done only once
 * in `print_markers()` for all the markers.
 *  @param[in] marker for a token.
 *  @param[in] src original source code.
 */
static void
print_marker(Marker_p m, Byte_array_p src)
{
  mut_Byte_array token_text = init_Byte_array(80);
  extract_src(m, m+1, src, &token_text);
  const char * const token = as_c_string(&token_text);
  switch (m->token_type) {
    case T_BLOCK_START: case T_TUPLE_START: case T_INDEX_START:
      eprint("“%s”", token);
      break;
    case T_BLOCK_END: case T_TUPLE_END: case T_INDEX_END:
      eprint("“%s”", token);
      break;
    case T_SPACE:
      eprint("“");
      for (size_t i = 0; i is_not token_text.len; ++i) {
        Byte c = *get_mut_Byte_array(&token_text, i);
        switch (c) {
          case '\n': eprint("\\n"); break;
          case '\r': eprint("\\r"); break;
          case '\t': eprint("\\t"); break;
          default:   eprint("%c", c);
        }
      }
      eprint("”");
      break;
    default:
      eprint("“%s”", token);
  }
  tabulate_eprint(m->len, markers_tabulator);
  eprintln(" ← %s%s",
           TokenType_STRING[m->token_type],
           m->synthetic? ", synthetic": "");
  destruct_Byte_array(&token_text);
}

/** Print a human-legible dump of the markers array to stderr.
 * Ignores the options about showing spaces/comments:
 * it is meant as a raw display of the markers array.
 *  @param[in] markers parsed program.
 *  @param[in] src original source code.
 *  @param[in] prefix string to be added at the beginning of the line.
 *  @param[in] start index to start with.
 *  @param[in] end   index to end with: one past the index of the last marker.
 */
static void
print_markers(Marker_array_p markers, Byte_array_p src, const char* prefix,
              size_t start, size_t end)
{
  if (end < start) end = start;

  size_t indent = 0;
  if (start is_not 0) {
    Marker_p m_end = get_Marker_array(markers, start);
    for (Marker_mut_p m = start_of_Marker_array(markers); m is_not m_end; ++m) {
      switch (m->token_type) {
        case T_BLOCK_START: case T_TUPLE_START: case T_INDEX_START:
          ++indent;
          break;
        case T_BLOCK_END: case T_TUPLE_END: case T_INDEX_END:
          --indent;
          break;
        default:
          break;
      }
    }
  }

  const char* token_number_format =
      markers->len <     10? "%s% 1lu: ":
      markers->len <    100? "%s% 2lu: ":
      markers->len <   1000? "%s% 3lu: ":
      markers->len <  10000? "%s% 4lu: ":
      markers->len < 100000? "%s% 5lu: ": "%s% 6lu: ";

  mut_Byte_array token_text = init_Byte_array(80);

  Marker_p markers_start = start_of_Marker_array(markers);
  Marker_p m_start = markers->start + start;
  Marker_p m_end   = markers->start + end;
  const char * token = NULL;
  for (Marker_mut_p m = m_start; m is_not m_end; ++m) {
    token_text.len = 0;
    extract_src(m, m+1, src, &token_text);
    token = as_c_string(&token_text);

    size_t len = m->len;

    eprint(token_number_format, prefix, (size_t)(m - markers_start));
    switch (m->token_type) {
      case T_BLOCK_START: case T_TUPLE_START: case T_INDEX_START:
        indent_eprint(indent++ * 2);
        eprint("“%s”", token);
        break;
      case T_BLOCK_END: case T_TUPLE_END: case T_INDEX_END:
        indent_eprint(--indent * 2);
        eprint("“%s”", token);
        break;
      case T_SPACE:
        indent_eprint(indent * 2);
        eprint("“");
        for (size_t i = 0; i is_not token_text.len; ++i) {
          Byte c = *get_mut_Byte_array(&token_text, i);
          switch (c) {
            case '\n': eprint("\\n"); ++len; break;
            case '\r': eprint("\\r"); ++len; break;
            case '\t': eprint("\\t"); ++len; break;
            default:   eprint("%c", c);
          }
        }
        eprint("”");
        break;
      default:
        indent_eprint(indent * 2);
        eprint("“%s”", token);
    }
    tabulate_eprint(len, markers_tabulator);
    eprintln(" ← %s%s",
             TokenType_STRING[m->token_type],
             m->synthetic? ", synthetic": "");
  }

  destruct_Byte_array(&token_text);
}

/** Call `print_markers()` in an area around the given cursor.
 */
static void
debug_cursor(Marker_p cursor, size_t radius, const char* label, Marker_array_p markers, Byte_array_p src) {
  eprintln("%s:", label);
  size_t i = index_Marker_array(markers, cursor);
  size_t start = i > radius? i - radius: 0;
  size_t end   = i+1 + radius;
  if (start > markers->len) start = markers->len;
  if (i     > markers->len) i     = markers->len;
  if (end   > markers->len) end   = markers->len;

  print_markers(markers, src, "  ", start, i    );
  print_markers(markers, src, "* ", i    , i + 1);
  print_markers(markers, src, "  ", i + 1, end  );
}

/** Match a keyword or identifier.
 *  @param[in] start of source code segment to search in.
 *  @param[in] end of source code segment.
 *
 *  See `enum TokenType` for a list of keywords.
 */
static inline TokenType
keyword_or_identifier(Byte_p start, Byte_p end)
{
  switch (end - start) {
    case 2:
      if (mem_eq(start, "do", 2)) {
        return T_CONTROL_FLOW_LOOP;
      }
      if (mem_eq(start, "if", 2)) {
        return T_CONTROL_FLOW_IF;
      }
      break;
    case 3:
      if (mem_eq(start, "for", 3)) {
        return T_CONTROL_FLOW_LOOP;
      }
      if (mem_eq(start, "int", 3)) {
        return T_TYPE;
      }
      break;
    case 4:
      if (mem_eq(start, "case", 4)) {
        return T_CONTROL_FLOW_CASE;
      }
      if (mem_eq(start, "else", 4)) {
        return T_CONTROL_FLOW_IF;
      }
      if (mem_eq(start, "goto", 4)) {
        return T_CONTROL_FLOW_GOTO;
      }
      if (mem_eq(start, "char", 4) or mem_eq(start, "enum", 4) or
          mem_eq(start, "long", 4) or mem_eq(start, "void", 4) or
          mem_eq(start, "bool", 4)) {
        return T_TYPE;
      }
      if (mem_eq(start, "auto", 4)) {
#ifdef USE_DEFER_AS_KEYWORD
        return T_TYPE_QUALIFIER_AUTO;
#else
        return T_CONTROL_FLOW_DEFER;
#endif
      }
      break;
    case 5:
      if (mem_eq(start, "break", 5)) {
        return T_CONTROL_FLOW_BREAK;
      }
      if (mem_eq(start, "while", 5)) {
        return T_CONTROL_FLOW_LOOP;
      }
      if (mem_eq(start, "float", 5) or mem_eq(start, "short", 5) or
          mem_eq(start, "union", 5)) {
        return T_TYPE;
      }
      if (mem_eq(start, "const", 5)) {
        return T_TYPE_QUALIFIER;
      }
#ifdef USE_DEFER_AS_KEYWORD
      if (mem_eq(start, "defer", 5)) {
        return T_CONTROL_FLOW_DEFER;
      }
#endif
      break;
    case 6:
      if (mem_eq(start, "return", 6)) {
        return T_CONTROL_FLOW_RETURN;
      }
      if (mem_eq(start, "switch", 6)) {
        return T_CONTROL_FLOW_SWITCH;
      }
      if (mem_eq(start, "double", 6) or mem_eq(start, "struct", 6)) {
        return T_TYPE_STRUCT;
      }
      if (mem_eq(start, "extern", 6) or mem_eq(start, "inline", 6) or
          mem_eq(start, "signed", 6) or mem_eq(start, "static", 6)){
        return T_TYPE_QUALIFIER;
      }
      if (mem_eq(start, "sizeof", 6)) {
        return T_OP_2;
      }
      break;
    case 7:
      if (mem_eq(start, "default", 7)) {
        return T_CONTROL_FLOW_CASE;
      }
      if (mem_eq(start, "typedef", 7)) {
        return T_TYPEDEF;
      }
      if (mem_eq(start, "complex", 7)) {
        return T_TYPE;
      }
      break;
    case 8:
      if (mem_eq(start, "continue", 8)) {
        return T_CONTROL_FLOW_CONTINUE;
      }
      if (mem_eq(start, "register", 8) or mem_eq(start, "restrict", 8) or
          mem_eq(start, "unsigned", 8) or mem_eq(start, "volatile", 8)){
        return T_TYPE_QUALIFIER;
      }
      if (mem_eq(start, "_Alignof", 8)) {
        return T_OP_2;
      }
      if (mem_eq(start, "_Generic", 8)) {
        return T_GENERIC_MACRO;
      }
      break;
    case 9:
      if (mem_eq(start, "imaginary", 9)) {
        return T_TYPE;
      }
      break;
  }
  return T_IDENTIFIER;
}

/** Wrap everything in the input up to `#pragma Cedro x.y` into a single token
 * for efficiency.
 *  Everything up to that marker is output verbatim without any processing,
 * so standard C files are by default not modified.
 *
 *  Example:
 * ```
 * mut_Marker_array markers = init_Marker_array(8192);
 * mut_Byte_array src = init_Byte_array(80);
 * push_str(&src, "#pragma Cedro 1.0\nprintf(\"hello\\n\", i);");
 * Byte_array_mut_slice region = bounds_of_Byte_array(&src);
 * region.start_p = parse_skip_until_cedro_pragma(&src, region, markers);
 * parse(&src, region, &markers);
 * print_markers(&markers, &src, "", 0, markers.len);
 * ```
 */
static Byte_p
parse_skip_until_cedro_pragma(Byte_array_p src, Byte_array_slice region, mut_Marker_array_p markers)
{
  assert(PADDING_Byte_ARRAY >= 8); // Must be greater than the longest keyword.
  Byte_mut_p cursor = region.start_p;
  Byte_p     end    = region.end_p;
  Byte_mut_p prev_cursor = NULL;

  // First look for the pragma.
  // We need to do some tokenization to avoid false positives if it appears
  // in a comment or string, for instance.
  while (cursor is_not end) {
    assert(cursor is_not prev_cursor);
    prev_cursor = cursor;

    Byte_mut_p token_end = NULL;
    if        ((token_end = preprocessor(cursor, end))) {
      if (CEDRO_PRAGMA_LEN < (size_t)(token_end - cursor)) {
        if (mem_eq((Byte_p)CEDRO_PRAGMA, cursor, CEDRO_PRAGMA_LEN)) {
          if (cursor is_not start_of_Byte_array(src)) {
            //if (*(cursor-1) is '\n') --cursor;
            mut_Marker inert;
            init_Marker(&inert, start_of_Byte_array(src), cursor, src, T_NONE);
            if (not push_Marker_array(markers, inert)) {
              error("OUT OF MEMORY ERROR.");
              return end;
            }
          }
          cursor = token_end;
          // Skip LF and empty lines after line.
          while (cursor is_not end and
                 ('\n' is *cursor or ' ' is *cursor)) ++cursor;
          break;
        }
      }
    } else if ((token_end = string    (cursor, end))) {
    } else if ((token_end = character (cursor, end))) {
    } else if ((token_end = comment   (cursor, end))) {
    } else if ((token_end = space     (cursor, end))) {
    } else if ((token_end = identifier(cursor, end))) {
    } else if ((token_end = number    (cursor, end))) {
    } else      token_end = other     (cursor, end);
    if (error_buffer[0]) {
      eprintln(LANG("Error: %lu: %s",
                    "Error: %lu: %s"),
               original_line_number((size_t)(cursor - src->start), src),
               error_buffer);
      error_buffer[0] = 0;
      return cursor;
    }
    cursor = token_end;
  }
  if (cursor is end) {
    // No “#pragma Cedro x.y”, so just wrap the whole C code verbatim.
    mut_Marker inert;
    init_Marker(&inert,
                start_of_Byte_array(src), end_of_Byte_array(src), src, T_NONE);
    if (not push_Marker_array(markers, inert)) {
      error("OUT OF MEMORY ERROR.");
      return end;
    }
  }

  return cursor;
}

#define TOKEN1(token) token_type = token
#define TOKEN2(token) ++token_end;    TOKEN1(token)
#define TOKEN3(token) token_end += 2; TOKEN1(token)

/** Parse the given source code into the `markers` array,
 * appending the new markers to whatever was already there.
 *
 *  Remember to empty the `markers` array before calling this function
 * if you are re-parsing from scratch.
 *
 *  Example:
 * ```
 * mut_Marker_array markers = init_Marker_array(8192);
 * mut_Byte_array src = init_Byte_array(80);
 * push_str(&src, "printf(\"hello\\n\", i);");
 * parse(&src, bounds_of_Byte_array(&src), &markers);
 * print_markers(&markers, &src, "", 0, markers.len);
 * ```
 *
 * Returns the point where parsing ended, which will normally be the
 * end of `region` unless there is an error, in which case the message
 * will be in `error_buffer`.
 */
static Byte_p
parse(Byte_array_p src, Byte_array_slice region, mut_Marker_array_p markers)
{
  assert(PADDING_Byte_ARRAY >= 8); // Must be greater than the longest keyword.
  Byte_mut_p cursor = region.start_p;
  Byte_p     end    = region.end_p;
  Byte_mut_p prev_cursor = NULL;
  bool previous_token_is_value = false;

  while (cursor is_not end) {
    assert(cursor is_not prev_cursor);
    prev_cursor = cursor;

    mut_TokenType token_type = T_NONE;
    Byte_mut_p token_end = NULL;
    if        ((token_end = preprocessor(cursor, end))) {
      if (cursor + CEDRO_PRAGMA_LEN < token_end and
          mem_eq(CEDRO_PRAGMA, cursor, CEDRO_PRAGMA_LEN)) {
        eprintln(
            LANG("Aviso: %lu: #pragma Cedro duplicada.\n"
                 "  puede hacer que algún código se malinterprete,\n"
                 "  por ejemplo si usa `auto` con su significado normal.",
                 "Warning: %lu: duplicated Cedro #pragma.\n"
                 "  This might cause some code to be misinterpreted,\n"
                 "  for instance if it uses `auto` in its standard meaning."),
            original_line_number((size_t)(cursor - src->start), src));
      } else if (cursor + 8/*strlen("#assert ")*/ <= token_end and
                 mem_eq("#assert ", cursor, 8/*strlen("#assert ")*/)) {
        error(LANG("La directiva #assert es incompatible con Cedro.",
                   "The #assert directive is incompatible with Cedro."));
        return cursor;
      } else if (cursor + 10/*strlen("#define };")*/ <= token_end and
                 mem_eq("#define };", cursor, 10/*strlen("#define };")*/)) {
        if (markers->len is_not 0) { // If 0, error will be caught by unparse().
          // https://gcc.gnu.org/onlinedocs/cpp/Swallowing-the-Semicolon.html
          Marker_mut_p semicolon =
              skip_space_back(markers->start, end_of_Marker_array(markers) - 1);
          if (semicolon is_not markers->start) {
            --semicolon;
            if (semicolon->token_type is T_SEMICOLON) {
              delete_Marker_array(markers,
                                  index_Marker_array(markers, semicolon),
                                  1);
            } else {
              error(LANG("la línea anterior debe terminar en punto y coma.",
                         "previous line must end in semicolon."));
            }
          }
        }
      }
      TOKEN1(T_PREPROCESSOR);
      if (token_end is cursor) { eprintln("error T_PREPROCESSOR"); break; }
    } else if ((token_end = string(cursor, end))) {
      TOKEN1(T_STRING);
      if (token_end is cursor) { eprintln("error T_STRING"); break; }
    } else if ((token_end = character(cursor, end))) {
      TOKEN1(T_CHARACTER);
      if (token_end is cursor) { eprintln("error T_CHARACTER"); break; }
    } else if ((token_end = comment(cursor, end))) {
      TOKEN1(T_COMMENT);
      if (token_end is cursor) { eprintln("error T_COMMENT"); break; }
    } else if ((token_end = space(cursor, end))) {
      TOKEN1(T_SPACE);
      if (token_end is cursor) { eprintln("error T_SPACE"); break; }
    } else if ((token_end = identifier(cursor, end))) {
      TOKEN1(keyword_or_identifier(cursor, token_end));
      if (token_end is cursor) { eprintln("error T_IDENTIFIER"); }
    } else if ((token_end = number(cursor, end))) {
      TOKEN1(T_NUMBER);
      if (token_end is cursor) { eprintln("error T_NUMBER"); break; }
    } else {
      uint8_t c = *cursor;
      token_end = cursor + 1;
      uint8_t c2 = *token_end;
      uint8_t c3 = *(token_end+1);
      switch (c) {
        case '{': TOKEN1(T_BLOCK_START); break;
        case '}': TOKEN1(T_BLOCK_END);   break;
        case '(': TOKEN1(T_TUPLE_START); break;
        case ')': TOKEN1(T_TUPLE_END);   break;
        case '[': TOKEN1(T_INDEX_START); break;
        case ']': TOKEN1(T_INDEX_END);   break;
        case ',': TOKEN1(T_COMMA);       break;
        case ';': TOKEN1(T_SEMICOLON);   break;
        case '.':
          if (c2 is '.' and c3 is '.') { TOKEN3(T_ELLIPSIS); }
          else if (c2 is '.')          { TOKEN2(T_ELLIPSIS); }
          else                         { TOKEN1(T_OP_1);     }
          break;
        case '~': TOKEN1(T_OP_2);        break;
        case '?': TOKEN1(T_OP_13);       break;
        case ':':
          switch (c2) {
            case '>': TOKEN2(T_INDEX_END); break;
            default:
              TOKEN1(T_OP_13); // Default.
              if (markers->len is_not 0) {
                Marker_p start =   start_of_Marker_array(markers);
                Marker_mut_p m = end_of_mut_Marker_array(markers);
                m = skip_space_back(start, m);
                if (m is_not start) --m;
                if (m->token_type is T_IDENTIFIER) {
                  mut_Marker_p label_candidate = (mut_Marker_p)m;
                  m = skip_space_back(start, m);
                  if (m is_not start) --m;
                  if (m->token_type is T_SEMICOLON   or
                      m->token_type is T_LABEL_COLON or
                      m->token_type is T_BLOCK_START or
                      m->token_type is T_BLOCK_END) {
                    label_candidate->token_type = T_CONTROL_FLOW_LABEL;
                    token_type = T_LABEL_COLON;
                  }
                } else {
                  mut_Error err = {0};
                  Marker_mut_p line_start = find_line_start(m, start, &err);
                  if (err.message) {
                    error(err.message);
                    return cursor;
                  }
                  while (line_start->token_type is T_SPACE or
                         line_start->token_type is T_COMMENT) ++line_start;
                  if (line_start->token_type is T_CONTROL_FLOW_CASE) {
                    token_type = T_LABEL_COLON;
                  }
                }
              }
          }
          break;
        case '+':
          switch (c2) {
            case '+':
              TOKEN2(T_OP_2);
              break;
            case '=': TOKEN2(T_OP_14);  break;
            default: // Binary form is T_OP_4.
              TOKEN1(previous_token_is_value? T_OP_4: T_OP_2);
          }
          break;
        case '-':
          switch (c2) {
            case '-':
              TOKEN2(T_OP_2);
              break;
            case '=': TOKEN2(T_OP_14);  break;
            case '>': TOKEN2(T_OP_2);   break;
            default: // Binary form is T_OP_4.
              TOKEN1(previous_token_is_value? T_OP_4: T_OP_2);
          }
          break;
        case '*':
          switch (c2) {
            case '=': TOKEN2(T_OP_14); break;
            default: // Prefix form is T_OP_2.
              TOKEN1(previous_token_is_value? T_OP_3: T_OP_2);
          }
          break;
        case '/':
        case '%':
          switch (c2) {
            case ':':
              /* %: → #, %:%: → ## */
              error(LANG("los digrafos %: y %:%: no están implementados.",
                         "the digraphs %: and %:%: are not implemented."));
              break;
            case '=': TOKEN2(T_OP_14); break;
            case '>': TOKEN2(T_BLOCK_END); break;
            default:  TOKEN1(T_OP_3);
          }
          break;
        case '=':
          switch (c2) {
            case '=': TOKEN2(T_OP_7); break;
            default:  TOKEN1(T_OP_14);
          }
          break;
        case '!':
          switch (c2) {
            case '=': TOKEN2(T_OP_7); break;
            default:  TOKEN1(T_OP_2);
          }
          break;
        case '>':
          switch (c2) {
            case '>':
              switch (c3) {
                case '=': TOKEN3(T_OP_14); break;
                default:  TOKEN2(T_OP_5);
              }
              break;
            case '=': TOKEN2(T_OP_6); break;
            default:  TOKEN1(T_OP_6);
          }
          break;
        case '<':
          switch (c2) {
            case '<':
              switch (c3) {
                case '=': TOKEN3(T_OP_14); break;
                default:  TOKEN2(T_OP_5);
              }
              break;
            case '=': TOKEN2(T_OP_6); break;
            case '%': TOKEN2(T_BLOCK_START); break;
            case ':': TOKEN2(T_INDEX_START); break;
            default:  TOKEN1(T_OP_6);
          }
          break;
        case '&':
          switch (c2) {
            case '&': TOKEN2(T_OP_11); break;
            case '=': TOKEN2(T_OP_14); break;
            default: // Prefix form is T_OP_2.
              TOKEN1(previous_token_is_value? T_OP_8: T_OP_2);
          }
          break;
        case '|':
          switch (c2) {
            case '|': TOKEN2(T_OP_12); break;
            case '=': TOKEN2(T_OP_14); break;
            default:  TOKEN1(T_OP_10);
          }
          break;
        case '^':
          switch (c2) {
            case '=': TOKEN2(T_OP_14); break;
            default:  TOKEN1(T_OP_9);
          }
          break;
        case '@':
          TOKEN1(T_BACKSTITCH);
          break;
        case '\\':
          if (cursor + 6 <= end and mem_eq(cursor, "\\u0040", 6)) {
            token_end += 5;
            token_type = T_OTHER;
          } else {
            TOKEN1(T_OTHER);
          }
          break;
      }
    }
    if (error_buffer[0]) {
      // This should never happen: any error must cause an immediate return.
      eprintln("Uncaught error: %s", error_buffer);
      exit(87);
      //return cursor;
    }

    if (token_type is T_NONE) {
      token_end = other(cursor, end);
      if (not token_end) {
        error(LANG("problema al extraer pedazo de tipo OTHER.",
                   "problem extracting token of type OTHER."));
        return cursor;
      }
    }

    mut_Marker marker;
    init_Marker(&marker, cursor, token_end, src, token_type);
    if (not push_Marker_array(markers, marker)) {
      error("OUT OF MEMORY ERROR.");
      return end;
    }
    cursor = token_end;

    switch (token_type) {
      case T_IDENTIFIER:
      case T_NUMBER:
      case T_STRING:
      case T_CHARACTER:
      case T_TUPLE_END:
      case T_INDEX_END:
        previous_token_is_value = true;
        break;
      case T_SPACE:
      case T_COMMENT:
        /* Not significant for this purpose. */
        break;
      default:
        previous_token_is_value = false;
    }
  }

  return cursor;
}

static inline bool
write_token(Marker_p m, Byte_array_p src, Options options, FILE* out)
{
  // Default token output:
  Byte_array_mut_slice text = slice_for_marker(src, m);
  if (options.escape_ucn and m->token_type is T_IDENTIFIER) {
    while (text.start_p is_not text.end_p) {
      uint32_t u = 0;
      UTF8Error err = UTF8_NO_ERROR;
      text.start_p = decode_utf8(text.start_p, text.end_p, &u, &err);
      if (utf8_error(err, (size_t)(text.start_p - src->start))) return false;
      if      ((u & 0xFFFFFF80) is 0 and
               u is_not 0x0024 and
               u is_not 0x0040 and
               u is_not 0x0060)       fputc((unsigned char)u, out);
      else if ((u & 0xFFFF0000) is 0) fprintf(out, "\\u%04X", u);
      else                            fprintf(out, "\\U%08X", u);
    }
  } else if (m->token_type is T_OTHER and m->len is 6 and
             mem_eq(get_Byte_array(src, m->start), "\\u0040", 6)) {
    putc('@', out);
  } else {
    fwrite(text.start_p, sizeof(text.start_p[0]), m->len, out);
  }

  return true;
}

typedef struct Replacement {
  Marker_mut_p marker;
  Marker_array_mut_slice replacement;
} MUT_CONST_TYPE_VARIANTS(Replacement);
DEFINE_ARRAY_OF(Replacement, 0, {});
static Marker_array_slice
get_replacement_value(Replacement_array_p _, Marker_p m, Byte_array_p src)
{
  Marker_array_mut_slice value = {0};
  Replacement_array_mut_slice r = bounds_of_Replacement_array(_);
  while (r.start_p is_not r.end_p) {
    if (is_same_token(r.start_p->marker, m, src)) {
      value = r.start_p->replacement;
      break;
    }
    ++r.start_p;
  }

  return value;
}

/**
 * @param[in] m marker for the `#include` line.
 */
typedef int (*IncludeCallbackFunction_p)(Marker_p m, Byte_array_p src,
                                         FILE* cc_stdin,
                                         void* const context);
typedef struct IncludeCallback {
  IncludeCallbackFunction_p function;
  void* context;
} MUT_CONST_TYPE_VARIANTS(IncludeCallback);

/* Prototype, defined after unparse_foreach(). */
static Marker_p
unparse_fragment(Marker_mut_p m, Marker_p m_end, size_t previous_marker_end,
                 Byte_array_p src, size_t original_src_len,
                 const char* src_file_name, IncludeCallback_p include,
                 mut_Replacement_array_p replacements, bool is_last,
                 Options options, FILE* out);
/**
 * There is no single-line variant `#foreach T {a_t, b_t, c_t} ...`.
 * Any `#foreach { ...` must be paired with another line `#foreach }`.
 * Assumes that the caller has already checked that `m`
 * starts with `#foreach {` exactly.
 * Therefore, `markers` can not be empty, but must contain at least one token.
 *
 * ```
 * #foreach { {T, N} {{unsigned char*, Byte}, {size_t, Index}}
 * ...
 * #foreach }
 * ```
 * Line continuations works as in other preprocessor directives:
 * ```
 * #foreach { {T, N} {{unsigned char*, Byte}, \
 *                    {size_t, Index}}
 * ...
 * #foreach }
 * ```
 * The values can also be put in lines below the directive:
 * ```
 * #foreach { {T, N}
 *   {{unsigned char*, Byte}, {size_t, Index}}
 * ...
 * #foreach }
 * ```
 * In that case, the values must start in that next line, not be split
 * between the directive line and the rest.
 */
static Marker_p
unparse_foreach(Marker_array_slice markers, size_t previous_marker_end,
                mut_Replacement_array_mut_p replacements,
                Byte_array_p src, size_t original_src_len,
                const char* src_file_name, IncludeCallback_p include,
                Options options, FILE* out)
{
  assert(markers.end_p > markers.start_p);
  Marker_mut_p m = markers.start_p;
  Marker_p m_end = markers.end_p;

  Byte_array_mut_slice text = slice_for_marker(src, m);
  Byte_mut_p rest = text.start_p + 10; // = strlen("#foreach {");

  size_t initial_replacements_len = replacements->len;

  mut_Marker_array arguments = init_Marker_array(32);
  Byte_p parse_end =
      parse(src, (Byte_array_slice){rest, text.end_p}, &arguments);
  if (parse_end is_not text.end_p) {
    if (fprintf(out, "#line %lu \"%s\"\n#error %s\n",
                original_line_number((size_t)(parse_end - src->start), src),
                src_file_name,
                error_buffer) < 0) {
      eprintln(LANG("error al escribir la directiva #line",
                    "error when writing #line directive"));
    }
    error_buffer[0] = 0;
    m = m_end;
    goto exit;
  }

  Marker_array_mut_slice arg = bounds_of_Marker_array(&arguments);
  arg.start_p = skip_space_forward(arg.start_p, arg.end_p);
  if (arg.start_p is arg.end_p) {
    write_error_at(LANG("error sintáctico.",
                        "syntax error."),
                   arg.start_p, src, out);
    m = m_end;
    goto exit;
  }
  switch (arg.start_p->token_type) {
    case T_IDENTIFIER:
      if (not push_Replacement_array(replacements, (Replacement){
            arg.start_p, {0}
          })) {
        error("OUT OF MEMORY ERROR.");
        return m_end;
      }
      ++arg.start_p;
      break;
    case T_BLOCK_START:
      ++arg.start_p;
      while (arg.start_p is_not arg.end_p) {
        arg.start_p = skip_space_forward(arg.start_p, arg.end_p);
        if (arg.start_p is arg.end_p) {
          write_error_at(LANG("error sintáctico.",
                              "syntax error."),
                         arg.start_p, src, out);
          m = m_end;
          goto exit;
        }
        if (arg.start_p->token_type is T_BLOCK_END) {
          if (replacements->len - initial_replacements_len < 2) {
            write_error_at(
                LANG("no se permiten llaves con una sola variable.",
                     "braces are not allowed with a single variable."),
                arg.start_p, src, out);
            m = m_end;
            goto exit;
          }
          ++arg.start_p;
          break;
        }
        if (arg.start_p->token_type is T_IDENTIFIER) {
          for (size_t i = 0; i < replacements->len; ++i) {
            if (is_same_token(arg.start_p,
                              replacements->start[i].marker,
                              src)) {
              write_error_at(LANG("argumento duplicado.",
                                  "duplicated argument."),
                             arg.start_p, src, out);
              m = m_end;
              goto exit;
            }
          }
          if (not push_Replacement_array(replacements,
                                         (Replacement){arg.start_p, {0}})) {
            error("OUT OF MEMORY ERROR.");
            return m_end;
          }
          ++arg.start_p;
          continue;
        }
        if (arg.start_p->token_type is_not T_COMMA) {
          write_error_at(LANG("error sintáctico, se esperaba una coma.",
                              "syntax error, expected a comma."),
                         arg.start_p, src, out);
          m = m_end;
          goto exit;
        }
        ++arg.start_p;
      }
      break;
    default:
      write_error_at(LANG("error sintáctico.",
                          "syntax error."),
                     arg.start_p, src, out);
      m = m_end;
      goto exit;
  }

  arg.start_p = skip_space_forward(arg.start_p, arg.end_p);
  if (arg.start_p is arg.end_p) {
    write_error_at(LANG("falta la lista de valores.",
                        "missing value list."),
                   arguments.start, src, out);
    m = m_end;
    goto exit;
  }

  if (arg.start_p->token_type is T_IDENTIFIER) {
    arg = get_replacement_value(replacements, arg.start_p, src);
  } else if (arg.start_p->token_type is_not T_BLOCK_START) {
    write_error_at(LANG("error sintáctico en lista de valores.",
                        "syntax error in value list."),
                   arguments.start, src, out);
    m = m_end;
    goto exit;
  }

  // Skip identifier or initial `{`:
  ++arg.start_p;
  // Skip spaces after it, but keep comments in the value:
  while (arg.start_p is_not arg.end_p and
         arg.start_p->token_type is T_SPACE) ++arg.start_p;

  if (arg.start_p is arg.end_p) {
    write_error_at(LANG("falta la lista de valores.",
                        "missing value list."),
                   m, src, out);
    m = m_end;
    goto exit;
  }

  Marker_mut_p fragment_end = m_end;

  while (arg.start_p is_not arg.end_p) {
    Marker_array_mut_slice value = (Marker_array_mut_slice){
      arg.start_p, arg.start_p
    };
    // Look for segment end.
    size_t nesting = 0;
    while (value.end_p is_not arg.end_p) {
      switch (value.end_p->token_type) {
        case T_BLOCK_START: case T_TUPLE_START: case T_INDEX_START:
          ++nesting;
          break;
        case T_BLOCK_END:
          if (not nesting) {
            if (skip_space_forward(value.end_p + 1, arg.end_p)
                is_not arg.end_p) {
              write_error_at(LANG("contenido inválido tras lista de valores",
                                  "invalid content after value list"),
                             value.start_p, src, out);
              m = m_end;
              goto exit;
            }
            goto found_args_end;
          }
          --nesting;
          break;
        case T_TUPLE_END: case T_INDEX_END:
          if (not nesting) {
            write_error_at(LANG("paréntesis desparejados en argumento.",
                                "unpaired parentheses in argument."),
                           value.start_p, src, out);
            m = m_end;
            goto exit;
          }
          --nesting;
          break;
        case T_COMMA:
          if (not nesting) goto found_args_end;
          break;
        default:
          break;
      }
      ++value.end_p;
    } found_args_end:
    if (nesting) {
      write_error_at(LANG("grupo sin cerrar, error sintáctico",
                          "unclosed group, syntax error."),
                     value.start_p, src, out);
      m = m_end;
      goto exit;
    }

    if (value.end_p is arg.end_p) {
      write_error_at(LANG("lista de valores inconclusa",
                          "unfinished value list"),
                     value.start_p, src, out);
      m = m_end;
      goto exit;
    }

    // Skip ending `,` or `}` and spaces after it.
    arg.start_p = skip_space_forward(value.end_p + 1, arg.end_p);

    // Trim space after segment, but not comments.
    while (value.end_p is_not value.start_p and
           (value.end_p - 1)->token_type is T_SPACE) --value.end_p;

    if (value.start_p is value.end_p) {
      write_error_at(LANG("valor vacío",
                          "empty value"),
                     value.end_p, src, out);
      m = m_end;
      goto exit;
    }

    size_t valueIndex = initial_replacements_len;
    if (replacements->len - initial_replacements_len is 1) {
      replacements->start[valueIndex].replacement = value;
    } else {
      // Look for segment end.
      Marker_array_mut_slice valueList = { value.start_p + 1, value.end_p - 1 };

      value.start_p = valueList.start_p;
      value.end_p   = valueList.start_p;
      nesting = 0;
      while (value.end_p < valueList.end_p) {
        switch (value.end_p->token_type) {
          case T_BLOCK_START: case T_TUPLE_START: case T_INDEX_START:
            ++nesting;
            break;
          case T_BLOCK_END:
            if (not nesting) {
              // This can not happen if the code above is correct.
              write_error_at(LANG("ERROR INTERNO EN CEDRO: 0x01",
                                  "INTERNAL ERROR IN CEDRO: 0x01"),
                             arg.start_p, src, out);
              m = m_end;
              goto exit;
            }
            --nesting;
            break;
          case T_TUPLE_END: case T_INDEX_END:
            if (not nesting) {
              write_error_at(LANG("paréntisis desparejados en argumento.",
                                  "unpaired parentheses in argument."),
                             value.start_p, src, out);
              m = m_end;
              goto exit;
            }
            --nesting;
            break;
          case T_COMMA:
            if (nesting) break;
            if (valueIndex is replacements->len) {
              write_error_at(LANG("mas valores que variables",
                                  "more values than variables"),
                             value.start_p, src, out);
              m = m_end;
              goto exit;
            }
            // Skip spaces, but not comments.
            while (value.start_p is_not value.end_p and
                   value.start_p->token_type is T_SPACE) ++value.start_p;
            if (value.start_p is value.end_p) {
              write_error_at(LANG("valor vacío",
                                  "empty value"),
                             value.end_p, src, out);
              m = m_end;
              goto exit;
            }

            replacements->start[valueIndex++].replacement = value;

            value.start_p = value.end_p + 1;
            // Skip spaces, but not comments.
            while (value.start_p is_not valueList.end_p and
                   value.start_p->token_type is T_SPACE) ++value.start_p;
            break;
          default:
            break;
        }
        ++value.end_p;
      }
      if (nesting) {
        // This can not happen if the code above is correct.
        write_error_at(LANG("ERROR INTERNO EN CEDRO: 0x02",
                            "INTERNAL ERROR IN CEDRO: 0x02"),
                       value.start_p, src, out);
        m = m_end;
        goto exit;
      }
      if (valueIndex is replacements->len) {
        write_error_at(LANG("mas valores que variables",
                            "more values than variables"),
                       value.start_p, src, out);
        m = m_end;
        goto exit;
      }

      if (value.start_p is value.end_p) {
        write_error_at(LANG("valor vacío",
                            "empty value"),
                       value.end_p, src, out);
        m = m_end;
        goto exit;
      }

      replacements->start[valueIndex++].replacement = value;

      if (valueIndex is_not replacements->len) {
        write_error_at(LANG("menos valores que variables",
                            "fewer values than variables"),
                       value.start_p, src, out);
        m = m_end;
        goto exit;
      }
      value.start_p = skip_space_forward(value.end_p+1, valueList.end_p);
    }
    assert(m is_not m_end);
    fragment_end = unparse_fragment(m + 1, m_end, previous_marker_end,
                                    src, original_src_len,
                                    src_file_name, include,
                                    replacements, arg.start_p is arg.end_p,
                                    options, out);
    if (error_buffer[0]) {
      m = m_end;
      goto exit;
    }
  }

  m = fragment_end;

exit:
  destruct_Marker_array(&arguments);
  if (replacements->len > initial_replacements_len) {
    truncate_Replacement_array(replacements, initial_replacements_len);
  }
  if (initial_replacements_len is 0) destruct_Replacement_array(replacements);

  return m;
}
/** Write pending space, and if there is a pending `#line` directive try
 * to insert it in the first available end of line.
 * @param line_directive_pending [out] will be set to `false` if it was
 * `true` and the `#line` directive was written.
 * @param src_file_name [in] C source file name, needed for `#line`.
 * @param pending_space [in] pointer to the pending space marker, not `NULL`.
 * @param src [in] source code as UTF-8 bytes.
 * @param options [in] program options.
 * @param out [in] stream where the result will be written.
 */
static bool
write_pending_space(bool* line_directive_pending, const char* src_file_name,
                    Marker_p pending_space, Marker_p end, Byte_array_p src,
                    Options options, FILE* out)
{
  if (*line_directive_pending) {
    Byte_array_mut_slice space = slice_for_marker(src, pending_space);
    Byte_mut_p insertion_point = space.end_p; // Right after the LF.
    if (insertion_point is_not space.start_p) {
      while (insertion_point is_not space.start_p) {
        Byte_mut_p next = insertion_point - 1;
        if (*next is '\n' and
            (next-1 is space.start_p or *(next-1) is_not '\\')) {
          break;
        }
        insertion_point = next;
      }
    }
    if (insertion_point is_not space.start_p or
        pending_space->token_type is T_NONE) {
      size_t len = (size_t)(insertion_point - space.start_p);
      if (fwrite(space.start_p, sizeof(space.start_p[0]), len, out)
          is_not len) {
        error(LANG("al escribir el espacio pendiente",
                   "when writing pending space"));
        return false;
      }
      size_t line_number = 0;
      if (pending_space is end) {
        line_number = original_line_number(pending_space->start + len, src);
      } else {
        // Skip plain spaces and synthetic tokens:
        Marker_mut_p next = pending_space + 1;
        while (next is_not end and
               (next->synthetic is true or
                (next->token_type is T_SPACE and
                 not has_byte('\n', next, src))
                )) {
          ++next;
        }
        if (next is_not end and next->synthetic is true) next = end;
        line_number = next is end? 0: original_line_number(next->start, src);
      }
      if (line_number is_not 0 and
          fprintf(out, "#line %lu \"%s\"\n", line_number, src_file_name) < 0) {
        error(LANG("al escribir la directiva #line",
                   "when writing #line directive"));
        return false;
      }
      *line_directive_pending = false;
      len = (size_t)(space.end_p - insertion_point);
      if (fwrite(insertion_point, sizeof(insertion_point[0]), len, out)
          is_not len) {
        error(LANG("al escribir el espacio pendiente",
                   "when writing pending space"));
        return false;
      }
      return true;
    }
  }

  return write_token(pending_space, src, options, out);
}
static Marker_p
unparse_fragment(Marker_mut_p m, Marker_p m_end, size_t previous_marker_end,
                 Byte_array_p src, size_t original_src_len,
                 const char* src_file_name, IncludeCallback_p include,
                 mut_Replacement_array_p replacements, bool is_last,
                 mut_Options options, FILE* out)
{
  if (m is m_end) return m_end;
  bool eol_pending = false;
  bool line_directive_pending = false;
  Marker_mut_p pending_space = NULL;
  while (m is_not m_end) {
    if (options.insert_line_directives) {
      line_directive_pending |= m->start is_not previous_marker_end;
      if (m->synthetic is_not true) previous_marker_end = m->start + m->len;
    }

    if (options.discard_comments and m->token_type is T_COMMENT) {
      if (options.discard_space and not eol_pending and
          m+1 is_not m_end and (m+1)->token_type is T_SPACE) {
        if (++m is m_end) break;
      }
      if (pending_space) {
        if (not write_pending_space(&line_directive_pending, src_file_name,
                                    pending_space, m_end, src,
                                    options, out)) {
          m = m_end;
          goto exit;
        }
        pending_space = NULL;
      }
      // Skipping the following T_SPACE token would work for a comment that
      // were alone in this line, but not if there is something in front of it.
      continue;
    }

    Byte_array_mut_slice text = slice_for_marker(src, m);
    Byte_mut_p rest = text.start_p;

    if (options.discard_space) {
      if (m->token_type is T_SPACE) {
        Byte_mut_p eol = text.start_p;
        size_t len = m->len;
        if (eol_pending) {
          while ((eol = memchr(eol, '\n', len))) {
            if (eol is text.start_p or *(eol-1) is_not '\\') {
              fputc('\n', out);
              eol_pending = false;
              break;
            }
            ++eol; // Skip the LF character we just found.
            len = m->len - (size_t)(eol - text.start_p);
          }
          if (not eol_pending) {
            ++m;
            continue;
          }
        }
        fputc(' ', out);
        pending_space = NULL;
        ++m;
        continue;
      } else if (m->token_type is T_PREPROCESSOR) {
        eol_pending = true;
      } else if (m->token_type is T_COMMENT) {
        // If not eol_pending, set it if this is a line comment.
        eol_pending = eol_pending or (m->len > 1 and '/' is text.start_p[1]);
      }
    }

    if (m->token_type is T_PREPROCESSOR and options.apply_macros) {
      UTF8Error err = UTF8_NO_ERROR;
      size_t len = 9;// = strlen("#define {")
      if (m->len >= len and strn_eq("#define {", (char*)rest, len)) {
        if (pending_space) {
          if (not write_pending_space(&line_directive_pending, src_file_name,
                                      pending_space, m_end, src,
                                      options, out)) {
            m = m_end;
            break;
          }
          pending_space = NULL;
        }
        rest += len;
        size_t line_length;
        if (rest is_not end_of_Byte_array(src) and *rest is ' ') {
          fputs("#define", out);
          line_length = 7;
        } else {
          fputs("#define ", out);
          line_length = 8;
        }
        fwrite(rest, sizeof(rest[0]), m->len - len, out);
        line_length += m->len - len;
        for (++m; m is_not m_end; ++m) {
          if (options.discard_comments and m->token_type is T_COMMENT) {
            continue;
          } else if (m->token_type is T_PREPROCESSOR) {
            text = slice_for_marker(src, m);
            rest = text.start_p;
            len = 9;// = strlen("#define }")
            if (m->len >= len and strn_eq("#define }", (char*)rest, len)) {
              fputs("/* End #define */", out);
              // Now check that there is only space and comments after it:
              rest += len;
              if (rest < text.end_p) {
                if (*rest is ';') ++rest; // No space allowed before ';'.
                Byte_mut_p end;
                while (rest is_not text.end_p) {
                  end = space(rest, text.end_p);
                  if (not end) end = comment(rest, text.end_p);
                  if (not end) break;
                  rest = end;
                }
                if (rest is_not text.end_p) {
                  write_error_at(LANG("contenido inválido tras `#define }`.",
                                      "invalid content after `#define }`"),
                                 m, src, out);
                  m = m_end;
                  goto exit;
                }
              }
              // Not needed: line_length += strlen("/* End #define */");
              ++m;
              break;
            }
            fwrite(rest, sizeof(rest[0]), m->len, out);
            line_length += len_utf8(text.start_p, text.end_p, &err);
            if (utf8_error(err, (size_t)(text.start_p - src->start))) {
              write_error_at(error_buffer, m, src, out);
              error_buffer[0] = 0;
              m = m_end;
              goto exit;
            }
          } else {
            text = slice_for_marker(src, m);
            rest = text.start_p;
            len = m->len;
            bool is_line_comment = false;
            if (m->token_type is T_COMMENT and len > 2) {
              // Valid comment tokens will always be at least 2 chars long,
              // but we need to check in case this one is not.
              if ('/' is rest[1]) {
                fputc('/', out); ++rest; --len; ++line_length;
                while ('/' is *rest and len) {
                  fputc('*', out); ++rest; --len; ++line_length;
                }
                is_line_comment = true;
              }
            }
            Byte_mut_p eol;
            while ((eol = memchr(rest, '\n', len))) {
              fwrite(rest, sizeof(rest[0]), (size_t)(eol - rest), out);
              line_length += len_utf8(rest, eol, &err);
              if (utf8_error(err, (size_t)(rest - src->start))) {
                write_error_at(error_buffer, m, src, out);
                error_buffer[0] = 0;
                m = m_end;
                goto exit;
              }
              if (is_line_comment) {
                fputs(" */", out);
                line_length += 3;
                is_line_comment = false;
              }
              fputc(' ', out);
              line_length += 1;

              fwrite(spacing, sizeof(spacing[0]),
                     line_length < right_margin? right_margin - line_length: 0,
                     out);
              fputs("\\\n", out);
              line_length = 0;

              len -= (size_t)(eol + 1 - rest);
              rest = eol + 1;
            }
            fwrite(rest, sizeof(rest[0]), len, out);
            line_length += len_utf8(rest, rest + len, &err);
            if (utf8_error(err, (size_t)(rest - src->start))) {
              write_error_at(error_buffer, m, src, out);
              error_buffer[0] = 0;
              m = m_end;
              goto exit;
            }
            if (is_line_comment) {
              fputs(" */", out);
              line_length += 3;
            }
          }
        }
        continue;
      }
      if (m->len >= len and strn_eq("#define }", (char*)rest, len)) {
        write_error_at(
            LANG("cierre de directiva de bloque sin apertura previa.",
                 "block directive closing without previous opening."),
            m, src, out);
        m = m_end;
        goto exit;
      }

      len = 10; // = strlen("#include {");
      if (m->len >= len and strn_eq("#include {", (char*)rest, len)) {
        if (pending_space) {
          if (not write_pending_space(&line_directive_pending, src_file_name,
                                      pending_space, m_end, src,
                                      options, out)) {
            m = m_end;
            break;
          }
          pending_space = NULL;
        }
        rest += len;
        Byte_mut_p end = rest;
        while (end < text.end_p) { if (*end is '}') break; ++end; }
        if (end is text.end_p) {
          write_error_at(LANG("falta la llave de cierre tras `#include {...`.",
                              "missing closing brace after `#include {...`"),
                         m, src, out);
          m = m_end;
          goto exit;
        }
        mut_Byte_array file_name = {0};
        if (not push_str(&file_name, src_file_name)) {
          error("OUT OF MEMORY ERROR.");
          return m_end;
        }
        Byte_array_mut_slice dirname = bounds_of_Byte_array(&file_name);
        while (dirname.end_p is_not dirname.start_p) {
          switch (*(dirname.end_p-1)) {
            case '/': case '\\': goto found_path_separator;
            default: --dirname.end_p;
          }
        } found_path_separator:
        file_name.len = len_Byte_array_slice(dirname);
        append_Byte_array(&file_name, (Byte_array_slice){ rest, end });
        const char* included_file = as_c_string(&file_name);
        mut_Byte_array bin = {0};
        int err = read_file(&bin, included_file);
        if (err) {
          print_file_error(err, included_file, &bin);
          fprintf(out, ";\n#error %s: %s\n", strerror(errno), included_file);
          destruct_Byte_array(&file_name);
          break;
        }
        if (bin.len is 0) {
          fprintf(out, ";\n#error file is empty: %s\n", included_file);
          destruct_Byte_array(&file_name);
          break;
        }
        const char* basename = strrchr(included_file, '/');
        if (not basename) basename = strrchr(included_file, '\\');
        if (basename) ++basename; else basename = included_file;
        fprintf(out, "[%lu] = { /* %s */\n0x%02X",
                bin.len, basename, bin.start[0]);
        for (size_t i = 1; i is_not bin.len; ++i) {
          fprintf(out, (i & 0x0F) is 0? ",\n0x%02X": ",0x%02X", bin.start[i]);
        }
        fputs("\n}", out);
        destruct_Byte_array(&bin);
        destruct_Byte_array(&file_name);
        // Now check that there is only space and comments after it:
        rest = end + 1;
        if (rest < text.end_p) {
          Byte_mut_p end;
          while (rest is_not text.end_p) {
            end = space(rest, text.end_p);
            if (not end) end = comment(rest, text.end_p);
            if (not end) break;
            rest = end;
          }
          if (rest is_not text.end_p) {
            write_error_at(LANG("contenido inválido tras `#include {...}`.",
                                "invalid content after `#include {...}`"),
                           m, src, out);
            m = m_end;
            goto exit;
          }
        }
        m = skip_space_forward(m + 1, m_end);
        continue;
      }

      len = 10;// = strlen("#foreach {") = strlen("#foreach }")
      if (m->len >= len) {
        if        (strn_eq("#foreach {", (char*)rest, len)) {
          bool insert_line_directives = options.insert_line_directives;
          if (pending_space) {
            // Similar to write_pending_space():
            Byte_array_mut_slice space = slice_for_marker(src, pending_space);
            while (space.end_p is_not space.start_p) {
              if (*--space.end_p is '\n') break;
            }
            fwrite(space.start_p, sizeof(space.start_p[0]),
                   (size_t)(space.end_p-space.start_p), out);
            pending_space = NULL;
            if (options.insert_line_directives and
                (space.end_p is_not space.start_p and
                 *(space.end_p - 1) is '\\')) {
              options.insert_line_directives = false;
            }
          }
          // Not used afterwards: rest += len;
          previous_marker_end = m->start;
          m = unparse_foreach((Marker_array_slice){ m, m_end },
                              previous_marker_end,
                              replacements,
                              src, original_src_len,
                              src_file_name, include,
                              options, out);
          previous_marker_end = m->start;
          line_directive_pending = options.insert_line_directives;
          options.insert_line_directives = insert_line_directives;
          continue;
        } else if (strn_eq("#foreach }", (char*)rest, len)) {
          line_directive_pending = false;
          if (pending_space) {
            Byte_array_mut_slice space = slice_for_marker(src, pending_space);
            while (space.end_p is_not space.start_p) {
              if (*--space.end_p is '\n') break;
            }
            fwrite(space.start_p, sizeof(space.start_p[0]),
                   (size_t)(space.end_p-space.start_p), out);
            pending_space = NULL;
          }
          // Now check that there is only space and comments after it:
          rest += len;
          if (rest < text.end_p) {
            Byte_mut_p end;
            while (rest is_not text.end_p) {
              end = space(rest, text.end_p);
              if (not end) end = comment(rest, text.end_p);
              if (not end) break;
              rest = end;
            }
            if (rest is_not text.end_p) {
              write_error_at(LANG("contenido inválido tras `#foreach }`.",
                                  "invalid content after `#foreach }`"),
                             m, src, out);
              m = m_end;
              goto exit;
            }
          }
          ++m; // Skip this token, point to T_SPACE newline after it.
          break;
        }
      }

      len = 8;// = strlen("#include")
      if (m->len >= len) {
        if (strn_eq("#include", (char*)rest, len) and include) {
          int result = include->function(m, src, out, include->context);
          if (result is -1) {
            // The included file is not a Cedro file, output the #include line.
          } else if (result is 0) {
            // The file was expanded.
            if (pending_space) {
              Byte_array_mut_slice space = slice_for_marker(src, pending_space);
              while (space.end_p is_not space.start_p) {
                if (*--space.end_p is '\n') break;
              }
              fwrite(space.start_p, sizeof(space.start_p[0]),
                     (size_t)(space.end_p-space.start_p), out);
              pending_space = NULL;
            }
            ++m;
            continue;
          } else {
            // TODO: improve error messages, report specific error.
            write_error_at(LANG("error en el `#include`.",
                                "`#include` error."),
                           m, src, out);
            m = m_end;
            goto exit;
          }
        }
      } else if (m->len is 2 and rest[1] is '#') {
        // Token concatenation like in the standard #define.
        // https://gcc.gnu.org/onlinedocs/cpp/Concatenation.html#Concatenation
        // Allow spaces before and after, as in #define:
        pending_space = NULL;
        m = skip_space_forward(m + 1, m_end);
        continue;
      } else if (m->len is 1 and replacements) {
        if (++m is m_end) break;
        // Token as string literal like in the standard #define.
        // https://gcc.gnu.org/onlinedocs/cpp/Stringizing.html#Stringizing
        // Cedro extension: conditional operator, e.g. `#,`
        // which is omitted in the last iteration of the loop.
        if (m is_not m_end and is_operator(m->token_type)) {
          if (not is_last) {
            if (pending_space) {
              if (not write_pending_space(&line_directive_pending,
                                          src_file_name,
                                          pending_space, m_end, src,
                                          options, out)) {
                m = m_end;
                goto exit;
              }
              pending_space = NULL;
            }
            if (not write_token(m, src, options, out)) {
              m = m_end;
              goto exit;
            }
          }
          ++m;
          continue;
        }
        if (m is m_end or m->token_type is_not T_IDENTIFIER) {
          write_error_at(LANG("falta el identificador after `#`.",
                              "missing the identifier after `#`."),
                         m, src, out);
          m = m_end;
          goto exit;
        }

        Marker_array_mut_slice value =
            get_replacement_value(replacements, m, src);

        if (not value.start_p) {
          error(LANG("falta el valor para la variable ",
                     "missing value for variable "));
          text = slice_for_marker(src, m);
          error_append((const char*)text.start_p, (const char*)text.end_p);
          write_error_at(error_buffer, m, src, out);
          error_buffer[0] = 0;
          m = m_end;
          goto exit;
        }

        if (pending_space) {
          if (not write_pending_space(&line_directive_pending, src_file_name,
                                      pending_space, m_end, src,
                                      options, out)) {
            m = m_end;
            goto exit;
          }
          pending_space = NULL;
        }
        fputc('"', out);
        for (Marker_mut_p m = value.start_p; m is_not value.end_p; ++m) {
          if (m->token_type is T_STRING) {
            for (Byte_array_mut_slice text = slice_for_marker(src, m);
                 text.start_p is_not text.end_p;
                 ++text.start_p) {
              Byte c = *text.start_p;
              if (c is '"' or c is '\\') fputc('\\', out);
              fputc(c, out);
            }
          } else if (not write_token(m, src, options, out)) {
            m = m_end;
            goto exit;
          }
        }
        fputc('"', out);
        ++m;
        continue;
      } else if (replacements and replacements->len is_not 0) {
        write_error_at(LANG("no se permiten directivas de preprocesador"
                            " dentro de `#foreach`.",
                            "preprocessor directives are not allowed"
                            " inside `#foreach`."),
                       m, src, out);
        m = m_end;
        goto exit;
      }
    } else if (replacements and m->token_type is T_IDENTIFIER) {
      if (pending_space) {
        if (not write_pending_space(&line_directive_pending, src_file_name,
                                    pending_space, m_end, src,
                                    options, out)) {
          m = m_end;
          goto exit;
        }
        pending_space = NULL;
      }
      // If token needs replacement, output replacement here instead.
      Marker_array_mut_slice value =
          get_replacement_value(replacements, m, src);

      if (value.start_p) {
        for (Marker_mut_p m = value.start_p; m is_not value.end_p; ++m) {
          if (not write_token(m, src, options, out)) {
            m = m_end;
            goto exit;
          }
        }
      } else {
        if (not write_token(m, src, options, out)) {
          m = m_end;
          goto exit;
        }
      }
      ++m;
      continue;
    }

    // Default token output:
    if (pending_space) {
      if (not write_pending_space(&line_directive_pending, src_file_name,
                                  pending_space, m_end, src,
                                  options, out)) goto exit;
      pending_space = NULL;
    }
    if (m->token_type is T_SPACE) {
      pending_space = m;
      ++m;
      continue;
    }
    if (not write_token(m, src, options, out)) goto exit;
    ++m;
  }

  if (pending_space) {
    if (not write_pending_space(&line_directive_pending, src_file_name,
                                pending_space, m_end, src,
                                options, out)) goto exit;
    pending_space = NULL;
  }

exit:
  return m;
}

/** Format the markers back into source code form.
 *  @param[in] markers tokens for the current form of the program.
 *  @param[in] src original source code, with any new tokens appended.
 *  @param[in] original_src_len original source code length.
 *  @param[in] src_file_name file name corresponding to `src`.
 *  @param[in] options formatting options.
 *  @param[out] out FILE pointer where the source code will be written.
 */
static void
unparse(Marker_array_slice markers,
        Byte_array_p src, size_t original_src_len,
        const char* src_file_name,
        Options options, FILE* out)
{
  assert(markers.end_p >= markers.start_p);

  Marker_mut_p m = markers.start_p;
  /* We need a special case because unparse_fragment()
   * does not have enough context to decide whether to insert it. */
  if (options.insert_line_directives and m->start is_not 0) {
    Marker empty = {0};
    bool line_directive_pending = true;
    if (not write_pending_space(&line_directive_pending, src_file_name,
                                &empty, markers.end_p, src,
                                options, out)) {
      return;
    }
  }

  mut_Replacement_array replacements = {0};
  unparse_fragment(m, markers.end_p, 0,
                   src, original_src_len,
                   src_file_name, NULL,
                   &replacements, false,
                   options, out);
  destruct_Replacement_array(&replacements); // Not needed, it’s a NOP.

  if (error_buffer[0]) {
    fprintf(out, "\n#error %s\n", error_buffer);
    error_buffer[0] = 0;
  }
}

typedef void (*MacroFunction_p)(mut_Marker_array_p markers,
                                mut_Byte_array_p src);
typedef const struct Macro {
  MacroFunction_p function;
  const char* name;
} Macro, * Macro_p;
#include "macros.h"
#define MACROS_DECLARE
static Macro macros[] = {
#include "macros.h"
  { NULL, NULL }
};
#undef  MACROS_DECLARE

#include <time.h>
/** Returns the time in seconds, as a double precision floating point value. */
static double
benchmark(mut_Byte_array_p src_p, const char* src_file_name, Options_p options)
{
  const size_t repetitions = 100;
  clock_t start = clock();

  mut_Marker_array markers = init_Marker_array(8192);

  for (size_t i = repetitions + 1; i; --i) {
    delete_Marker_array(&markers, 0, markers.len);

    Byte_array_mut_slice region = bounds_of_Byte_array(src_p);
    region.start_p = parse_skip_until_cedro_pragma(src_p, region, &markers);
    Byte_p parse_end = parse(src_p, region, &markers);
    if (parse_end is_not region.end_p) {
      destruct_Marker_array(&markers);
      eprintln("#line %lu \"%s\"\n#error %s\n",
               original_line_number((size_t)(parse_end - src_p->start), src_p),
               src_file_name,
               error_buffer);
      error_buffer[0] = 0;
      return 0.0; // Error.
    }

    if (options->apply_macros) {
      Macro_p macro = macros;
      while (macro->name and macro->function) {
        macro->function(&markers, src_p);
        ++macro;
      }
    }
    fputc('.', stderr);
  }

  clock_t end = clock();
  destruct_Marker_array(&markers);
  return ((double)(end - start))/CLOCKS_PER_SEC / (double) repetitions;
}

/** Validates equivalence of input file to the given reference file.
 * It does not apply any macros, just tokenizes both inputs.
 */
static bool
validate_eq(mut_Byte_array_p src, mut_Byte_array_p src_ref,
            const char* src_file_name, const char* src_ref_file_name,
            Options_p options)
{
  bool result = true;
  mut_Marker_array markers     = init_Marker_array(8192);
  mut_Marker_array markers_ref = init_Marker_array(8192);

  /* Do not apply macros or anything else, just a straight tokenization. */
  Byte_array_mut_slice region;
  Byte_mut_p parse_end;
  region    = bounds_of_Byte_array(src);
  parse_end = parse(src, region, &markers);
  if (parse_end is_not region.end_p) {
    result = false;
    eprintln("#line %lu \"%s\"\n#error %s\n",
             original_line_number((size_t)(parse_end - src->start), src),
             src_file_name,
             error_buffer);
    error_buffer[0] = 0;
    goto exit;
  }
  region    = bounds_of_Byte_array(src_ref);
  parse_end = parse(src_ref, region, &markers_ref);
  if (parse_end is_not region.end_p) {
    result = false;
    eprintln("#line %lu \"%s\"\n#error %s\n",
             original_line_number((size_t)(parse_end-src_ref->start), src_ref),
             src_ref_file_name,
             error_buffer);
    error_buffer[0] = 0;
    goto exit;
  }

  Marker_p
      end     = end_of_Marker_array(&markers),
      end_ref = end_of_Marker_array(&markers_ref);
  Marker_mut_p
      cursor     = start_of_Marker_array(&markers),
      cursor_ref = start_of_Marker_array(&markers_ref);
  cursor     = skip_space_forward(cursor,     end);
  cursor_ref = skip_space_forward(cursor_ref, end_ref);
  while (cursor is_not end and cursor_ref is_not end_ref) {
    if (cursor->token_type is_not cursor_ref->token_type or
        cursor->len > cursor_ref->len                    or
        not mem_eq(get_Byte_array(src,     cursor    ->start),
                   get_Byte_array(src_ref, cursor_ref->start),
                   cursor->len)) {
      result = false;
      break;
    }
    cursor     = skip_space_forward(cursor     + 1, end);
    cursor_ref = skip_space_forward(cursor_ref + 1, end_ref);
  }

  if (result is false) {
    mut_Byte_array message = init_Byte_array(80);
    if (not push_fmt(&message, LANG("Procesado, línea %lu",
                                    "Processed, line %lu"),
                     original_line_number(cursor->start, src))) {
      error("OUT OF MEMORY ERROR.");
      return false;
    }
    debug_cursor(cursor, 5, as_c_string(&message), &markers, src);
    if (message.len) truncate_Byte_array(&message, 0);
    if (not push_fmt(&message, LANG("Referencia, línea %lu",
                                    "Reference, line %lu"),
                     original_line_number(cursor_ref->start, src_ref))) {
      error("OUT OF MEMORY ERROR.");
      return false;
    }
    debug_cursor(cursor_ref, 5, as_c_string(&message), &markers_ref, src_ref);
    destruct_Byte_array(&message);
  }
exit:
  destruct_Marker_array(&markers_ref);
  destruct_Marker_array(&markers);
  return result;
}


#ifndef USE_CEDRO_AS_LIBRARY

static const char* const
usage_es =
    "Uso: cedro [opciones] <fichero.c>…\n"
    "     cedro new <nombre> # Ejecuta: cedro-new <nombre>\n"
    "  Para leer desde stdin, se pone - en vez de <fichero.c>.\n"
    "  El resultado va a stdout, se puede compilar sin fichero intermedio:\n"
    " cedro fichero.c | cc -x c - -o fichero\n"
    "  Es lo que hace el programa cedrocc:\n"
    " cedrocc -o fichero fichero.c\n"
    "  Con cedrocc, las siguientes opciones son implícitas:\n"
    "    --discard-comments --insert-line-directives\n"
    "\n"
    "  --apply-macros     Aplica las macros: pespunte, diferido, etc. (implícito)\n"
    "  --escape-ucn       Encapsula los caracteres no-ASCII en identificadores.\n"
    "  --no-apply-macros  No aplica las macros.\n"
    "  --no-escape-ucn    No encapsula caracteres en identificadores. (implícito)\n"
    "  --discard-comments    Descarta los comentarios.\n"
    "  --discard-space       Descarta los espacios en blanco.\n"
    "  --no-discard-comments No descarta los comentarios. (implícito)\n"
    "  --no-discard-space    No descarta los espacios.    (implícito)\n"
    "  --insert-line-directives    Inserta directivas #line.\n"
    "  --no-insert-line-directives No inserta directivas #line. (implícito)\n"
    "\n"
    "  --print-markers    Imprime los marcadores.\n"
    "  --no-print-markers No imprime los marcadores. (implícito)\n"
    "  --benchmark        Realiza una medición de rendimiento.\n"
    "  --validate=ref.c   Compara el resultado con el fichero «ref.c» dado.\n"
    "      No aplica las macros: para comparar el resultado de aplicar Cedro\n"
    "      a un fichero, pase la salida a través de esta opción, por ejemplo:\n"
    "      cedro fichero.c | cedro - --validate=ref.c\n"
    "  --version          Muestra la versión: " CEDRO_VERSION "\n"
    "                     El «pragma» correspondiente es: #pragma Cedro "
    CEDRO_VERSION
    ;
static const char* const
usage_en =
    "Usage: cedro [options] <file.c>…\n"
    "       cedro new <name> # Runs: cedro-new <name>\n"
    "  To read from stdin, put - instead of <file.c>.\n"
    "  The result goes to stdout, can be compiled without intermediate files:\n"
    " cedro file.c | cc -x c - -o file\n"
    "  It is what the cedrocc program does:\n"
    " cedrocc -o file file.c\n"
    "  With cedrocc, the following options are the defaults:\n"
    "    --discard-comments --insert-line-directives\n"
    "\n"
    "  --apply-macros     Apply the macros: backstitch, defer, etc. (default)\n"
    "  --escape-ucn       Escape non-ASCII in identifiers as UCN.\n"
    "  --no-apply-macros  Does not apply the macros.\n"
    "  --no-escape-ucn    Does not escape non-ASCII in identifiers. (default)\n"
    "  --discard-comments    Discards the comments.\n"
    "  --discard-space       Discards all whitespace.\n"
    "  --no-discard-comments Does not discard comments.   (default)\n"
    "  --no-discard-space    Does not discard whitespace. (default)\n"
    "  --insert-line-directives    Insert #line directives.\n"
    "  --no-insert-line-directives Does not insert #line directives."
    " (default)\n"
    "\n"
    "  --print-markers    Prints the markers.\n"
    "  --no-print-markers Does not print the markers. (default)\n"
    "  --benchmark        Run a performance benchmark.\n"
    "  --validate=ref.c   Compares the input to the given “ref.c” file.\n"
    "      Does not apply any macros: to compare the result of running Cedro\n"
    "      on a file, pipe its output through this option, for instance:\n"
    "      cedro file.c | cedro - --validate=ref.c\n"
    "  --version          Show version: " CEDRO_VERSION "\n"
    "                     The corresponding “pragma” is: #pragma Cedro "
    CEDRO_VERSION
    ;

int main(int argc, char** argv)
{
  int err = 0;

  if (argc > 2 and str_eq("new", argv[1])) {
    mut_Byte_array cmd = init_Byte_array(80);
    if (not (push_str(&cmd, argv[0]) && push_str(&cmd, "-new"))) {
      error("OUT OF MEMORY ERROR.");
      return ENOMEM;
    }
    for (int i = 2; i < argc; ++i) {
      if (not (push_str(&cmd, " ") && push_str(&cmd, argv[i]))) {
        error("OUT OF MEMORY ERROR.");
        return ENOMEM;
      }
    }
    err = system(as_c_string(&cmd));
    destruct_Byte_array(&cmd);
    return err;
  }

  mut_Options options = { // Remember to keep the usage strings updated.
    .apply_macros           = true,
    .escape_ucn             = false,
    .discard_comments       = false,
    .discard_space          = false,
    .insert_line_directives = false
  };

  bool opt_print_markers    = false;
  bool opt_run_benchmark    = false;
  const char* opt_validate  = NULL;

  for (int i = 1; i < argc; ++i) {
    char* arg = argv[i];
    if (arg[0] is '-' and arg[1] is_not '\0') {
      bool flag_value = !strn_eq("--no-", arg, 6);
      if        (str_eq("--apply-macros", arg) or
                 str_eq("--no-apply-macros", arg)) {
        options.apply_macros = flag_value;
      } else if (str_eq("--escape-ucn", arg) or
                 str_eq("--no-escape-ucn", arg)) {
        options.escape_ucn = flag_value;
      } else if (str_eq("--discard-comments", arg) or
                 str_eq("--no-discard-comments", arg)) {
        options.discard_comments = flag_value;
      } else if (str_eq("--discard-space", arg) or
                 str_eq("--no-discard-space", arg)) {
        options.discard_space = flag_value;
      } else if (str_eq("--insert-line-directives", arg) or
                 str_eq("--no-insert-line-directives", arg)) {
        options.insert_line_directives = flag_value;
      } else if (str_eq("--print-markers", arg) or
                 str_eq("--no-print-markers", arg)) {
        opt_print_markers = flag_value;
      } else if (str_eq("--benchmark", arg)) {
        opt_run_benchmark = true;
      } else if (strn_eq("--validate=", arg, strlen("--validate="))) {
        opt_validate = arg + strlen("--validate=");
      } else if (str_eq("--version", arg)) {
        eprintln(CEDRO_VERSION);
      } else {
        eprintln(LANG(usage_es, usage_en));
        err = str_eq("-h", arg) or str_eq("--help", arg)? 0: 1;
        return err;
      }
    }
  }

  if (opt_run_benchmark) {
    options.apply_macros = false;
    opt_print_markers    = false;
  }

  mut_Marker_array markers = init_Marker_array(8192);
  mut_Byte_array src = init_Byte_array(16384);

  FILE* out = stdout;

  for (int i = 1; not err and i < argc; ++i) {
    char* file_name = argv[i];
    if (file_name[0] is '-') {
      if (file_name[1] is_not '\0') continue;
      file_name[0] = '\0'; // Make file_name the empty string.
    } else if (file_name[0] is '\0') {
      // Do not allow `cedro ""` as alternative for `cedro -` because it is
      // likely to be a mistake, e.g. an undefined variable in a shell script.
      fprintf(out, "#error The file name is the empty string.\n");
      break;
    }

    // Re-use arrays:
    markers.len = 0;
    src.len = 0;

    int err = file_name[0]?
        read_file(&src, file_name):
        read_stream(&src, stdin);
    if (err) {
      print_file_error(err, file_name, &src);
      if (file_name[0] is '\0') {
        fprintf(out, "#error The file name is the empty string.\n");
      }
      err = 11;
      break;
    }

    Byte_array_mut_slice region = bounds_of_Byte_array(&src);
    region.start_p = parse_skip_until_cedro_pragma(&src, region, &markers);
    Byte_p parse_end = parse(&src, region, &markers);
    if (parse_end is_not region.end_p) {
      err = 1;
      eprintln("#line %lu \"%s\"\n#error %s\n",
               original_line_number((size_t)(parse_end - src.start), &src),
               file_name,
               error_buffer);
      error_buffer[0] = 0;
      break;
    }

    size_t original_src_len = src.len;

    if (opt_run_benchmark) {
      double t = benchmark(&src, file_name, &options);
      if (t < 1.0) eprintln("%.fms for %s", t * 1000.0, file_name);
      else         eprintln("%.1fs for %s", t         , file_name);
    } else if (opt_validate) {
      mut_Byte_array src_ref = {0};
      err = read_file(&src_ref, opt_validate);
      if (err) {
        print_file_error(err, opt_validate, &src_ref);
        err = 12;
      } else if (not validate_eq(&src,      &src_ref,
                                 file_name, opt_validate,
                                 &options)) {
        err = 27;
      }
      destruct_Byte_array(&src_ref);
    } else {
      if (options.apply_macros) {
        Macro_p macro = macros;
        while (macro->name and macro->function) {
          macro->function(&markers, &src);
          ++macro;
        }
      }

      if (opt_print_markers) {
        print_markers(&markers, &src, "", 0, markers.len);
      } else {
        unparse(bounds_of_Marker_array(&markers),
                &src, original_src_len,
                file_name,
                options, out);
      }
    }

    fflush(out);
  }

  fflush(out);
  destruct_Byte_array(&src);
  destruct_Marker_array(&markers);

  return err;
}

#endif // USE_CEDRO_AS_LIBRARY
