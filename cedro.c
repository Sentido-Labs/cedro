/* -*- coding: utf-8 c-basic-offset: 2 tab-width: 2 indent-tabs-mode: nil -*- */
/** \file */
/** \mainpage
 * The Cedro C pre-processor has three features:
 * the *backstitch* operator,
 * a simple resource release *defer* functionality,
 * and *block* (or multi-line) macros.
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
 * Array utilities: [array.h](array_8h.html)
 *
 * \author Alberto González Palomo https://sentido-labs.com
 * \copyright ©2021 Alberto González Palomo https://sentido-labs.com
 *
 * Created: 2020-11-25 22:41
 */
#define CEDRO_VERSION "1.0"
/** Versions with the same major number are compatible in that they produce
 * semantically equivalent output: there might be differeces in indentation
 * etc. but will be the same after parsing by the compiler.
 */
#define CEDRO_PRAGMA "#pragma Cedro 1."
#define CEDRO_PRAGMA_LEN 16

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <iso646.h>
#define is_not not_eq
#define is     ==
#define in(min, x, max) (x >= min && x <= max)
#include <string.h> // For memcpy(), memmove().
#define mem_eq(a, b, len)   (0 is memcmp(a, b, len))
#define str_eq(a, b)        (0 is strcmp(a, b))
#define strn_eq(a, b, len)  (0 is strncmp(a, b, len))

#include <assert.h>
#include <sys/resource.h>
#include <errno.h>

#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"

/** Same as `fprintf(stderr, fmt, ...), fputc('\n', stderr)`
 * but converting UTF-8 characters to Latin-1 if the `LANG` environment
 * variable does not contain `UTF-8`. */
static void
eprintln(const char * const fmt, ...)
{
  va_list args;
  va_start(args, fmt);

  if (strstr(getenv("LANG"), "UTF-8")) {
    vfprintf(stderr, fmt, args);
  } else {
    char* buffer;
    char small[512];
    size_t needed = (size_t) vsnprintf(small, sizeof(small), fmt, args);
    if (needed <= sizeof(small)) {
      buffer = &small[0];
    } else {
      buffer = malloc(needed);
      if (!buffer) {
        fprintf(stderr, "OUT OF MEMORY ERROR. %lu BYTES NEEDED.\n", needed);
        return;
      }
      vsnprintf(buffer, needed, fmt, args);
    }
    char* end = buffer + needed;
    char* p = buffer;
    char c;
    while ((c = *p)) {
      uint32_t u = 0;
      uint32_t len = 0;
      if      (0x00 == (c & 0x80)) { u = (uint32_t)(c       ); len = 1; }
      else if (0xC0 == (c & 0xE0)) { u = (uint32_t)(c & 0x1F); len = 2; }
      else if (0xE0 == (c & 0xF0)) { u = (uint32_t)(c & 0x0F); len = 3; }
      else if (0xF0 == (c & 0xF8)) { u = (uint32_t)(c & 0x07); len = 4; }
      if (len is 0 or p + len >= end) {
        fprintf(stderr, "UTF-8 DECODE ERROR AT %lu.\n",
                (size_t)(p - buffer));
        return;
      }
      switch (len) {
        case 4: c = *(++p); if (!c) break; u = (u << 6) | (c & 0x3F);
        case 3: c = *(++p); if (!c) break; u = (u << 6) | (c & 0x3F);
        case 2: c = *(++p); if (!c) break; u = (u << 6) | (c & 0x3F);
        case 1:      (++p);
      }
      /* Not a problem in this case:
      if (len is 2 and u <    0x80 or
          len is 3 and u <  0x0800 or
          len is 4 and u < 0x10000) {
        fprintf(stderr, "UTF-8 DECODE ERROR AT %lu, OVERLONG SEQUENCE.\n",
                (size_t)(p - buffer));
        return;
      }
      if (u >= 0xD800 and u < 0xE000) {
        fprintf(stderr, "UTF-8 DECODE ERROR AT %lu, SURROGATE CODE POINT.\n",
                (size_t)(p - buffer));
        return;
      }
      */
      if ((u & 0xFFFFFF00) is 0) {
        fputc((unsigned char) u, stderr); // ISO-8859-1/ISO-8859-15
      } else {
        switch (u) {
          case 0x0000201C:
          case 0x0000201D: fputc('"',   stderr); break;
          case 0x00002019: fputc('\'',  stderr); break;
          case 0x00002026: fputs("...", stderr); break;
          default:         fputc('_',   stderr);
        }
      }
    }
    if (buffer != &small[0]) free(buffer);
  }
  fputc('\n', stderr);

  va_end(args);
}
#define LANG(es, en) (strn_eq(getenv("LANG"), "es", 2)? es: en)

/** Defines mutable struct types mut_〈T〉 and mut_〈T〉_p (pointer),
 * and constant types 〈T〉 and 〈T〉_p (pointer to constant).
 */
#define TYPEDEF_STRUCT(T, TYPE)                                          \
  typedef struct T TYPE mut_##T, * const mut_##T##_p, * mut_##T##_mut_p; \
  typedef const struct T      T, * const       T##_p, *       T##_mut_p
/** Defines mutable types mut_〈T〉 and mut_〈T〉_p (pointer),
 * and constant types 〈T〉 and 〈T〉_p (pointer to constant).
 *
 *  You can define similar types for an existing type, for instance `uint8_t`:
 * ```
 * typedef       uint8_t mut_Byte, * const mut_Byte_p, *  mut_Byte_mut_p;
 * typedef const uint8_t     Byte, * const     Byte_p, *      Byte_mut_p;
 * ```
 */
#define TYPEDEF(T, TYPE)                                                 \
  typedef     TYPE mut_##T, * const mut_##T##_p, * mut_##T##_mut_p;      \
  typedef const TYPE     T, * const       T##_p, *       T##_mut_p

/** Parameters set by command line options. */
typedef struct Options {
  /// Whether to skip space tokens, or include them in the markers array.
  bool discard_space;
  /// Skip comments, or include them in the markers array.
  bool discard_comments;
  /// Escape Unicode® code points (“chararacters”) in identifiers
  /// as universal character names (ISO/IEC 9899:TC3 Annex D).
  bool escape_ucn;
  /// Apply the macros.
  bool apply_macros;
  /// Print markers array.
  bool print_markers;
} Options, *Options_p;

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
  /** Number, either integer or float.
      See `number()`.                                 */ T_NUMBER,
  /** String including the quotes: `"ABC"`            */ T_STRING,
  /** Character including the apostrophes: ```'A'```  */ T_CHARACTER,
  /** Whitespace, a block of `SP`, `HT`, `LF` or `CR`.
      See [Wikipedia, Basic ASCII control codes](https://en.wikipedia.org/wiki/C0_and_C1_control_codes#Basic_ASCII_control_codes)     */ T_SPACE,
  /** Comment block or line.                          */ T_COMMENT,
  /** Preprocessor directive.                         */ T_PREPROCESSOR,
  /** Generic macro.                                  */ T_GENERIC_MACRO,
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
  /** Backstitch: `@`                                */ T_BACKSTITCH,
  /** Ellipsis: `...`                                */ T_ELLIPSIS,
  /** Other token that is not part of the C grammar. */ T_OTHER
} mut_TokenType, * const mut_TokenType_p, *  mut_TokenType_mut_p;
typedef const enum TokenType
/*  */TokenType, * const     TokenType_p, *      TokenType_mut_p;

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
  B("Control flow goto"),
  B("Number"), B("String"), B("Character"),
  B("Space"), B("Comment"),
  B("Preprocessor"), B("Generic macro"),
  B("Block start"), B("Block end"), B("Tuple start"), B("Tuple end"),
  B("Index start"), B("Index end"),
  B("Group start"), B("Group end"),
  B("Op 1"), B("Op 2"), B("Op 3"), B("Op 4"), B("Op 5"), B("Op 6"),
  B("Op 7"), B("Op 8"), B("Op 9"), B("Op 10"), B("Op 11"), B("Op 12"),
  B("Op 13"), B("Op 14"),
  B("Comma (op 15)"), B("Semicolon"),
  B("Backstitch"),
  B("Ellipsis"),
  B("OTHER")
};

#define precedence(token_type) (token_type - T_OP_1)
#define is_operator(token_type) (token_type >= T_OP_1 && token_type <= T_COMMA)
#define is_fence(token_type) (token_type >= T_BLOCK_START && token_type <= T_GROUP_END)

/** Marks a C token in the source code. */
TYPEDEF_STRUCT(Marker, {
    size_t start;             /**< Start position, in bytes/chars. */
    uint32_t len;             /**< Length, in bytes/chars. */
    mut_TokenType token_type; /**< Token type. */
  });

/** Error while processing markers. */
TYPEDEF_STRUCT(Error, {
    Marker_mut_p position; /**< Position at which the problem was noticed. */
    const char * message;  /**< Message for user. */
  });

#include "array.h"

DEFINE_ARRAY_OF(Marker, 0, {});
DEFINE_ARRAY_OF(TokenType, 0, {});

TYPEDEF(Byte, uint8_t);
/** Add 8 bytes after end of buffer to avoid bounds checking while scanning
 * for tokens. No literal token is longer. */
DEFINE_ARRAY_OF(Byte, 8, {});

/** Append the C string bytes to the end of the given buffer. */
static void
push_str(mut_Byte_array_p _, const char * const str)
{
  Byte_array_slice insert = {
    .start_p = (Byte_p) str,
    .end_p   = (Byte_p) str + strlen(str)
  };
  splice_Byte_array(_, _->len, 0, NULL, &insert);
}

/** Append a formatted C string to the end of the given buffer.
 *  It’s the same as printf(...), only the result is stored instead
 * of printed to stdout. */
static void
push_fmt(mut_Byte_array_p _, const char * const fmt, ...)
{
  va_list args;
  va_start(args, fmt);

  ensure_capacity_Byte_array(_, _->len + 80);
  size_t available = _->capacity - _->len;
  size_t needed = (size_t) vsnprintf((char*) Byte_array_end(_), available - 1,
                                     fmt, args);
  if (needed > available) {
    ensure_capacity_Byte_array(_, _->len + needed + 1);
    available = _->capacity - _->len;
    vsnprintf((char*) Byte_array_end(_), available - 1,
              fmt, args);
  }
  _->len += needed;

  va_end(args);
}

/** Make it be a valid C string, by adding a `'\0'` character after the last
 * last valid element, without increasing the length.
 *  It first makes sure there is at least one extra byte after the array
 * contents in memory, by increasing the capacity if needed, and then
 * sets it to `0`.
 *  Returns the pointer `_->items` which now can be used as a C string
 * as long as you don’t modify it.
 */
static const char *
as_c_string(mut_Byte_array_p _)
{
  if (_->len is _->capacity) {
    push_Byte_array(_, '\0');
    --_->len;
  } else {
    *((mut_Byte_p) _->items + _->len) = 0;
  }
  return (const char *) _->items;
}

typedef FILE* mut_File_p;
typedef char*const FilePath;
/** Read a file into the given buffer. Errors are printed to `stderr`. */
static void
read_file(mut_Byte_array_p _, FilePath path)
{
  mut_File_p input = fopen(path, "r");
  if (!input) {
    fprintf(stderr,
            LANG("Error al abrir el fichero: «%s»: 0x%02X %s.\n",
                 "Error when opening the file: “%s”: 0x%02X %s.\n"),
            path, errno, strerror(errno));
    init_Byte_array(_, 0);
    return;
  }
  fseek(input, 0, SEEK_END);
  size_t size = (size_t)ftell(input);
  init_Byte_array(_, size);
  rewind(input);
  _->len = fread((mut_Byte_p) _->items, sizeof(_->items[0]), size, input);
  if (feof(input)) {
    fprintf(stderr,
            LANG("Fin de fichero inesperado en %ld leyendo «%s».\n",
                 "Unexpected EOF at %ld reading “%s”.\n"),
            _->len, path);
  } else if (ferror(input)) {
    fprintf(stderr,
            LANG("Error leyendo «%s»: 0x%02X %s.\n",
                 "Error reading “%s”: 0x%02X %s.\n"),
            path, errno, strerror(errno));
  } else {
    memset((mut_Byte_p) _->items + _->len, 0,
           (_->capacity - _->len) * sizeof(_->items[0]));
  }
  fclose(input); input = NULL;
}

static mut_Byte_array error = { 0 };// 0 in all bytes is a valid initialization.

/** Initialize a marker with the given values. */
static void
init_Marker(mut_Marker_p _, Byte_p start, Byte_p end, Byte_array_p src,
            TokenType token_type)
{
  _->start      = index_Byte_array(src, start);
  _->len        = (size_t)(end - start);
  _->token_type = token_type;
}

/* Lexer definitions. */

/** Match an identifier.
 * Assumes `end` > `start`. */
static inline Byte_p
identifier(Byte_p start, Byte_p end)
{
  Byte_mut_p cursor = start;
  mut_Byte c = *cursor;
  uint32_t u = 0;
  uint32_t len = 0;
  if      (0x00 == (c & 0x80)) { u = (uint32_t)(c       ); len = 1; }
  else if (0xC0 == (c & 0xE0)) { u = (uint32_t)(c & 0x1F); len = 2; }
  else if (0xE0 == (c & 0xF0)) { u = (uint32_t)(c & 0x0F); len = 3; }
  else if (0xF0 == (c & 0xF8)) { u = (uint32_t)(c & 0x07); len = 4; }
  if (len is 0 or cursor + len >= end) {
    error.len = 0;
    push_fmt(&error, "UTF-8 DECODE ERROR, BYTE IS 0x%02X.", c);
    return NULL;
  }
  switch (len) {
    case 4: c = *(++cursor); u = (u << 6) | (c & 0x3F);
    case 3: c = *(++cursor); u = (u << 6) | (c & 0x3F);
    case 2: c = *(++cursor); u = (u << 6) | (c & 0x3F);
    case 1:      (++cursor);
  }
  if (len is 2 and u <    0x80 or
      len is 3 and u <  0x0800 or
      len is 4 and u < 0x10000) {
    error.len = 0;
    push_fmt(&error, "UTF-8 DECODE ERROR, OVERLONG SEQUENCE.");
    return NULL;
  }
  if (u is '\\') {
    if (cursor is_not end) {
      switch(*cursor) {
        case 'U': len = 8; u = 0; break;
        case 'u': len = 4; u = 0; break;
        default:
          --cursor;
          return cursor is start? NULL: cursor;
      }
      if (cursor + len >= end) {
        error.len = 0;
        push_fmt(&error, "Incomplete universal character name.");
        return NULL;
      }
      while (len is_not 0) {
        --len;
        c = *(++cursor);
        Byte value =
            in('0',c,'9')? c-'0':
            in('A',c,'F')? c-'A'+10:
            in('a',c,'f')? c-'a'+10:
            0xFF;
        if (value is 0xFF) {
          error.len = 0;
          push_fmt(&error, "Malformed universal character name.");
          return NULL;
        }
        u = (u << 4) | value;
      }
    } else {
      return cursor is start? NULL: cursor;
    }
  }
  if (u >= 0xD800 and u < 0xE000) {
    error.len = 0;
    push_fmt(&error, "UTF-8 DECODE ERROR, SURROGATE CODE POINT.");
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
    while (cursor < end) {
      Byte_mut_p p = cursor;
      c = *p;
      u = 0;
      len = 0;
      if      (0x00 == (c & 0x80)) { u = (uint32_t)(c       ); len = 1; }
      else if (0xC0 == (c & 0xE0)) { u = (uint32_t)(c & 0x1F); len = 2; }
      else if (0xE0 == (c & 0xF0)) { u = (uint32_t)(c & 0x0F); len = 3; }
      else if (0xF0 == (c & 0xF8)) { u = (uint32_t)(c & 0x07); len = 4; }
      if (len is 0 or p + len >= end) {
        error.len = 0;
        push_fmt(&error, "UTF-8 DECODE ERROR.");
        return NULL;
      }
      switch (len) {
        case 4: c = *(++p); u = (u << 6) | (c & 0x3F);
        case 3: c = *(++p); u = (u << 6) | (c & 0x3F);
        case 2: c = *(++p); u = (u << 6) | (c & 0x3F);
        case 1:      (++p);
      }
      if (len is 2 and u <    0x80 or
          len is 3 and u <  0x0800 or
          len is 4 and u < 0x10000) {
        error.len = 0;
        push_fmt(&error, "UTF-8 DECODE ERROR, OVERLONG SEQUENCE.");
        return NULL;
      }
      if (u is '\\') {
        if (p is_not end) {
          switch(*p) {
            case 'U': len = 8; u = 0; break;
            case 'u': len = 4; u = 0; break;
            default:
              --p;
              return p is start? NULL: p;
          }
          if (p + len >= end) {
            error.len = 0;
            push_fmt(&error, "Incomplete universal character name.");
            return NULL;
          }
          while (len is_not 0) {
            --len;
            c = *(++p);
            Byte value =
                in('0',c,'9')? c-'0':
                in('A',c,'F')? c-'A':
                in('a',c,'F')? c-'a':
                0xFF;
            if (value is 0xFF) {
              error.len = 0;
              push_fmt(&error, "Malformed universal character name.");
              return NULL;
            }
            u = (u << 4) | value;
          }
          eprintln("Decoded following character: 0x%X", u);
        }
      }
      if (u >= 0xD800 and u < 0xE000) {
        error.len = 0;
        push_fmt(&error, "UTF-8 DECODE ERROR, SURROGATE CODE POINT.");
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
 * This matches invalid numbers like 3.4.6, 09, and 3e23.48.34e+11.
 * Rejecting that is left to the compiler.
 * Assumes `end` > `start`. */
static inline Byte_p
number(Byte_p start, Byte_p end)
{
  Byte_mut_p cursor = start;
  mut_Byte c = *cursor;
  if (c >= '1' and c <= '9') {
    ++cursor;
    while (cursor < end) {
      c = *cursor;
      if (in('0',c,'9') or c is '.') {
        ++cursor;
      } else if (c is 'u' or c is 'U' or c is 'l' or c is 'L') {
        ++cursor;
        if (cursor < end) {
          c = *cursor;
          if (c is 'u' or c is 'U' or c is 'l' or c is 'L') ++cursor;
          else break;
        }
      } else if (c is 'e' or c is 'E') {
        ++cursor;
        if (cursor < end) {
          c = *cursor;
          if (c is '+' or c is '-') ++cursor;
          while (cursor < end) {
            c = *cursor;
            if (in('1',c,'9') or c is '.') ++cursor;
            else break;
          }
        }
      } else break;
    }
    return cursor;
  } else {
    return NULL;
  }
}

/** Match a string literal.
 * Assumes `end` > `start`. */
static inline Byte_p
string(Byte_p start, Byte_p end)
{
  if (*start is_not '"') return NULL;
  Byte_mut_p cursor = start + 1;
  while (cursor is_not end and *cursor is_not '"') {
    if (*cursor is '\\' && cursor + 1 is_not end) ++cursor;
    ++cursor;
  }
  return (cursor is end)? NULL: cursor + 1;// End is past the closing symbol.
}

/** Match a character literal.
 * Assumes `end` > `start`. */
static inline Byte_p
character(Byte_p start, Byte_p end)
{
  if (*start is_not '\'') return NULL;
  Byte_mut_p cursor = start + 1;
  while (cursor is_not end and *cursor is_not '\'') {
    if (*cursor is '\\' && cursor + 1 is_not end) ++cursor;
    ++cursor;
  }
  return (cursor is end)? NULL: cursor + 1;// End is past the closing symbol.
}

/** Match whitespace: one or more space, `TAB`, `CR`, or `NL` characters.
 * Assumes `end` > `start`. */
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
        // No need to check bounds thanks to PADDING_Byte_array.
        if (*(cursor + 1) is_not '\n') goto exit;
        ++cursor;
        break;
      default:
        goto exit;
        // Unreachable: break;
    }
  }
exit:
  return (cursor is start)? NULL: cursor;
}

/** Match a comment block.
 * Assumes `end` > `start`. */
static inline Byte_p
comment(Byte_p start, Byte_p end)
{
  if (*start is_not '/') return NULL;
  Byte_mut_p cursor = start + 1;
  if (*cursor is '/') {
    do {
      cursor = memchr(cursor + 1, '\n', (size_t)(end - cursor));
      if (!cursor) { cursor = end; break; }
    } while ('\\' == *(cursor - 1));
    return cursor; // The newline is not part of the token.
  } else if (*cursor is_not '*') {
    return NULL;
  }
  ++cursor; // Skip next character, at minimum a '*' if the comment is empty.
  if (cursor is end) return NULL;
  do {
    cursor = memchr(cursor + 1, '/', (size_t)(end - cursor));
    if (!cursor) { cursor = end; break; }
  } while ('*' != *(cursor - 1));
  return (cursor is end)? end: cursor + 1;// Token includes the closing symbol.
}

/** Match a pre-processor directive.
 * Assumes `end` > `start`. */
static inline Byte_p
preprocessor(Byte_p start, Byte_p end)
{
  if (*start is_not '#') return NULL;
  Byte_mut_p cursor = start;
  do {
    cursor = memchr(cursor + 1, '\n', (size_t)(end - cursor));
    if (!cursor) { cursor = end; break; }
  } while ('\\' == *(cursor - 1));
  return cursor; // The newline is not part of the token.
}

/** Fallback match, just read one Unicode Code Point
 * as one token of type `T_OTHER`.
 * Assumes `end` > `start`. */
static inline Byte_p
other(Byte_p start, Byte_p end)
{
  mut_Byte c = *start;
  if      (0x00 == (c & 0x80)) { return start + 1; }
  else if (0xC0 == (c & 0xE0)) { return start + 2; }
  else if (0xE0 == (c & 0xF0)) { return start + 3; }
  else if (0xF0 == (c & 0xF8)) { return start + 4; }
  else                           return NULL;
}

/** Get new slice for the given marker.
 */
static Byte_array_slice
slice_for_marker(Byte_array_p src, Marker_p cursor)
{
  Byte_array_slice slice = {
    // get_mut_Byte_array() is not valid at the end.
    .start_p = get_mut_Byte_array(src, cursor->start),
    .end_p   = get_mut_Byte_array(src, cursor->start) + cursor->len
  };
  return slice;
}

/** Extract the indentation of the line for the character at `index`,
 * including the preceding `LF` character if it exists.
 */
static Marker
indentation(Byte_array_p src, Marker_p marker)
{
  Byte_p start = Byte_array_start(src);
  Byte_p end   = Byte_array_end(src);
  Byte_mut_p start_of_line = slice_for_marker(src, marker).start_p;
  while (*start_of_line is_not '\n') {
    if (start_of_line is start) break;
    --start_of_line;
  }
  // If *start_of_line is not '\n' here, there is no LF to re-use.
  // This can only happen at the very fist line of the file,
  // which should have no indentation anyway.
  // Handling the hypothetical case where the first line of the file is indented
  // is not worth the complication.
  Byte_mut_p end_of_indentation = start_of_line + 1;
  // TODO: this no longer makes sense: this is a single token of type T_SPACE.
  while (end_of_indentation is_not end) {
    if (*end_of_indentation is_not ' ' &&
        *end_of_indentation is_not '\t') break;
    ++end_of_indentation;
  }
  mut_Marker indentation;
  init_Marker(&indentation, start_of_line, end_of_indentation, src, T_SPACE);
  return indentation;
}

/** Copy the characters between `start` and `end` into the given Byte array,
 * appending them to the end of that Byte array.
 * If you want to replace any previous content,
 * do `string.len = 0;` before calling this function.
 *
 *  To extract the text for `Marker_p m` from `Byte_p src`:
 * ```
 * mut_Byte_array string; init_Byte_array(&string, 20);
 * extract_src(m, m + 1, src, &string);
 * destruct_Byte_array(&string);
 * ```
 *  If you want string.items to be a zero-terminated C string:
 * ```
 * push_Byte_array(&string, '\0');
 * ```
 *  or use `as_c_string(mut_Byte_array_p _)`.
 *
 *  @param[in] start marker pointer.
 *  @param[in] end marker pointer.
 *  @param[in] src byte buffer with the source code.
 *  @param[out] string Byte buffer to receive the bytes copied from the segment.
 */
static void
extract_src(Marker_p start, Marker_p end, Byte_array_p src, mut_Byte_array_p string)
{
  Marker_mut_p cursor = start;
  while (cursor < end) {
    Byte_array_slice insert = slice_for_marker(src, cursor);
    splice_Byte_array(string, string->len, 0, NULL, &insert);
    ++cursor;
  }
}

/** Count appearances of the given byte in a marker. */
static size_t
count_appearances(Byte byte, Marker_p start, Marker_p end, Byte_array_p src)
{
  if (end == start) return 0;
  size_t count = 0;
  Marker_mut_p cursor = end - 1;
  while (cursor is_not start) {
    Byte_array_slice src_segment = slice_for_marker(src, cursor);
    Byte_mut_p pointer = src_segment.start_p;
    while ((pointer =
            memchr(pointer, byte, (size_t)(src_segment.end_p - pointer) ))) {
      ++pointer;
      ++count;
    }
    --cursor;
  }
  return count;
}

/** Compute the line number as
 * ```1 + count_line_ends_between(src, 0, position)```.
 */
static size_t
line_number(Byte_array_p src, Marker_array_p markers, Marker_p position)
{
  return 1 + count_appearances('\n',
                               Marker_array_start(markers),
                               position,
                               src);
}

/** Indent by the given number of double spaces, for the next `eprintln()`
 * or `fprintf(stder, …)`. */
static void
indent_eprintln(size_t indent)
{
  if (indent > 40) indent = 40;
  while (indent-- is_not 0) fprintf(stderr, "  ");
}
/** Log to `stderr` with the given indentation. */
#define eprintln_indent(indent, ...) { indent_eprintln(indent); eprintln(__VA_ARGS__); }

/** Build a new marker for the given string,
 * pointing to its first appearance in `src`.
 *  If not found, append the text to `src`
 * and return a marker poiting there.
 */
static Marker
new_marker(mut_Byte_array_p src, const char * const text, TokenType token_type)
{
  Byte_mut_p cursor = Byte_array_start(src);
  Byte_mut_p match  = NULL;
  Byte_p     end    = Byte_array_end(src);
  size_t text_len = strlen(text);
  if (end - cursor >= text_len) {
    const char first_character = text[0];
    while ((cursor = memchr(cursor, first_character, (size_t)(end - cursor)))) {
      Byte_mut_p p1 = cursor;
      Byte_mut_p p2 = (Byte_p) text;
      while (*p2 && p1 is_not end && *p1 is *p2) { ++p1; ++p2; }
      if (*p2 is 0) {
        match = cursor;
        break;
      }
      ++cursor;
    }
  }
  mut_Marker marker;
  if (not match) {
    match = Byte_array_end(src);
    Byte_array_slice insert = {
      .start_p = (Byte_p)text,
      .end_p   = (Byte_p)text + text_len
    };
    splice_Byte_array(src, src->len, 0, NULL, &insert);
  }
  init_Marker(&marker, match, match + text_len, src, token_type);

  return marker;
}

/** Print a human-legible dump of the markers array to stderr.
 *  @param[in] markers parsed program.
 *  @param[in] src original source code.
 *  @param[in] start index to start with.
 *  @param[in] end   index to end with: if `0`, use the end of the array.
 *  @param[in] options formatting options.
 */
static void
print_markers(Marker_array_p markers, Byte_array_p src,
              size_t start, size_t end,
              Options options)
{
  size_t indent = 0;
  mut_Byte_array token_text;
  init_Byte_array(&token_text, 80);

  const char * const spacing = "                                ";
  const size_t spacing_len = strlen(spacing);

  Marker_p m_start =
      get_Marker_array(markers, start);
  Marker_p m_end   = end?
      get_Marker_array(markers, end):
      Marker_array_end(markers);
  const char * token = NULL;
  for (Marker_mut_p m = m_start; m is_not m_end; ++m) {
    token_text.len = 0;
    extract_src(m, m+1, src, &token_text);
    token = as_c_string(&token_text);

    const char * const spc = m->len <= spacing_len?
        spacing + m->len:
        spacing + spacing_len;

    switch (m->token_type) {
      case T_BLOCK_START: case T_TUPLE_START: case T_INDEX_START:
        eprintln_indent(indent++, "%s %s← %s",
                        token, spc, TokenType_STRING[m->token_type]);
        break;
      case T_BLOCK_END: case T_TUPLE_END: case T_INDEX_END:
        eprintln_indent(--indent, "%s %s← %s",
                        token, spc, TokenType_STRING[m->token_type]);
        break;
      case T_GROUP_START: case T_GROUP_END:
        /* Invisible grouping of tokens. */
        break;
      case T_SPACE:
        if (not options.discard_space) {
          eprintln_indent(indent, "%s %s← Space",
                          token, spc);
        }
        break;
      case T_COMMENT:
        if (not options.discard_comments) {
          eprintln_indent(indent, "%s %s← Comment",
                          token, spc);
        }
        break;
      default:
        eprintln_indent(indent, "%s %s← %s",
                        token, spc, TokenType_STRING[m->token_type]);
    }
  }

  destruct_Byte_array(&token_text);
}

/** Format the markers back into source code form.
 *  @param[in] markers tokens for the current form of the program.
 *  @param[in] src original source code.
 *  @param[in] options formatting options.
 *  @param[in] out FILE pointer where the source code will be written.
 */
static void
unparse(Marker_array_p markers, Byte_array_p src, Options options, FILE* out)
{
  Marker_p m_end = Marker_array_end(markers);
  bool eol_pending = false;
  for (Marker_mut_p m = (Marker_mut_p) markers->items; m is_not m_end; ++m) {
    if (options.discard_comments && m->token_type is T_COMMENT) {
      if (options.discard_space && not eol_pending &&
          m+1 is_not m_end && (m+1)->token_type is T_SPACE) ++m;
      continue;
    }
    Byte_mut_p text = slice_for_marker(src, m).start_p;
    if (options.discard_space) {
      if (m->token_type is T_SPACE) {
        Byte_mut_p eol = text;
        size_t len = m->len;
        if (eol_pending) {
          while ((eol = memchr(eol, '\n', len))) {
            if (eol is text || *(eol-1) is_not '\\') {
              fputc('\n', out);
              eol_pending = false;
              break;
            }
            ++eol; // Skip the LF character we just found.
            len = m->len - (size_t)(eol - text);
          }
          if (not eol_pending) continue;
        }
        fputc(' ', out);
        continue;
      } else if (m->token_type is T_PREPROCESSOR) {
        eol_pending = true;
      } else if (m->token_type is T_COMMENT) {
        // If not eol_pending, set it if this is a line comment.
        eol_pending = eol_pending || (m->len > 1 && '/' is text[1]);
      }
    }
    if (m->token_type is T_PREPROCESSOR) {
      size_t len = 9; // = strlen("#define {");
      if (m->len >= len and
          strn_eq("#define {", (char*)text, m->len < len? m->len: len)) {
        text += len;
        if (text is_not Byte_array_end(src) && *text == ' ') {
          fputs("#define", out);
        } else {
          fputs("#define ", out);
        }
        fwrite(text, sizeof(src->items[0]), m->len - len, out);
        for (++m; m is_not m_end; ++m) {
          if (options.discard_comments && m->token_type is T_COMMENT) {
            continue;
          } else if (m->token_type is T_PREPROCESSOR) {
            text = slice_for_marker(src, m).start_p;
            if (strn_eq("#define }", (char*)text, m->len < 9? m->len: 9)) {
              puts("// End #define");
              break;
            }
            fwrite(text, sizeof(src->items[0]), m->len, out);
          } else {
            text = slice_for_marker(src, m).start_p;
            len = m->len;
            bool is_line_comment = false;
            if (m->token_type is T_COMMENT and len > 2) {
              // Valid comment tokens will always be at least 2 chars long,
              // but we need to check in case this one is not.
              if ('/' == text[1]) {
                fputc('/', out); ++text; --len;
                while ('/' == *text and len) {
                  fputc('*', out); ++text; --len;
                }
                is_line_comment = true;
              }
            }
            Byte_mut_p eol;
            while ((eol = memchr(text, '\n', len))) {
              fwrite(text, sizeof(src->items[0]), (size_t)(eol - text), out);
              if (is_line_comment) {
                fputs(" */", out);
                is_line_comment = false;
              }
              fputs(" \\\n", out);
              len -= (size_t)(eol - text) + 1;
              text = eol + 1;
            }
            fwrite(text, sizeof(src->items[0]), len, out);
            if (is_line_comment) {
              fputs(" */", out);
            }
          }
        }
        continue;
      }
      len = 10; // = strlen("#include {");
      if (m->len >= len and
          strn_eq("#include {", (char*)text, m->len < len? m->len: len)) {
        size_t end;
        for (end = len; end < m->len; ++end) if (text[end] is '}') break;
        char* file_name = malloc(end - len + 1);
        memcpy(file_name, text + len, end - len);
        file_name[end - len] = '\0';
        mut_Byte_array bin;
        read_file(&bin, file_name);
        if (not bin.len) {
          fprintf(out, ";// No data for %s\n#error %s",
                  file_name, strerror(errno));
          break;
        }
        char*          basename = strrchr(file_name, '/');
        if (!basename) basename = strrchr(file_name, '\\');
        if (basename) ++basename; else basename = file_name;
        fprintf(out, "[%lu] = { // %s\n0x%2X",
                bin.len, basename, bin.items[0]);
        for (size_t i = 1; i is_not bin.len; ++i) {
          fprintf(out,
                  (i & 0x0F) is 0? ",\n0x%02X": ",0x%02X", bin.items[i]);
        }
        fputs("\n}", out);
        destruct_Byte_array(&bin);

        if (m+1 is_not m_end and (m+1)->token_type is T_SPACE) ++m;
        continue;
      }
    }
    if (options.escape_ucn and m->token_type is T_IDENTIFIER) {
      Byte_p end = text + m->len;
      Byte_mut_p p = text;
      while ((p is_not end)) {
        mut_Byte c = *p;
        uint32_t u = 0;
        uint32_t len = 0;
        if      (0x00 == (c & 0x80)) { u = (uint32_t)(c       ); len = 1; }
        else if (0xC0 == (c & 0xE0)) { u = (uint32_t)(c & 0x1F); len = 2; }
        else if (0xE0 == (c & 0xF0)) { u = (uint32_t)(c & 0x0F); len = 3; }
        else if (0xF0 == (c & 0xF8)) { u = (uint32_t)(c & 0x07); len = 4; }
        if (len is 0 or p + len > end) {
          fprintf(out, "len=%d, p=%p, end=%p\n", len, p, end);
          fprintf(out, "UTF-8 DECODE ERROR, BYTE IS 0x%02X.\n", c);
          return;
        }
        switch (len) {
          case 4: c = *(++p); if (!c) break; u = (u << 6) | (c & 0x3F);
          case 3: c = *(++p); if (!c) break; u = (u << 6) | (c & 0x3F);
          case 2: c = *(++p); if (!c) break; u = (u << 6) | (c & 0x3F);
          case 1:      (++p);
        }
        // No need for UTF-8 overlong encoding error checks here
        // because they were done already when parsing the file.
        if      ((u & 0xFFFFFF80) is 0) fputc(c, out);
        else if ((u & 0xFFFF0000) is 0) fprintf(out, "\\u%04X", u);
        else                            fprintf(out, "\\U%08X", u);
      }
    } else {
      fwrite(text, sizeof(src->items[0]), m->len, out);
    }
  }
}

/** Match a keyword or identifier.
 *  @param[in] start of source code segment to search in.
 *  @param[in] end of source code segment.
 *
 *  See `enum TokenType` for a list of keywords.
 */
static TokenType
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
      if (mem_eq(start, "char", 4) || mem_eq(start, "enum", 4) ||
          mem_eq(start, "long", 4) || mem_eq(start, "void", 4) ||
          mem_eq(start, "bool", 4)) {
        return T_TYPE;
      }
      if (mem_eq(start, "auto", 4)) {
        return T_TYPE_QUALIFIER_AUTO;
      }
      break;
    case 5:
      if (mem_eq(start, "break", 5)) {
        return T_CONTROL_FLOW_BREAK;
      }
      if (mem_eq(start, "while", 5)) {
        return T_CONTROL_FLOW_LOOP;
      }
      if (mem_eq(start, "float", 5) || mem_eq(start, "short", 5) ||
          mem_eq(start, "union", 5)) {
        return T_TYPE;
      }
      if (mem_eq(start, "const", 5)) {
        return T_TYPE_QUALIFIER;
      }
      break;
    case 6:
      if (mem_eq(start, "return", 6)) {
        return T_CONTROL_FLOW_RETURN;
      }
      if (mem_eq(start, "switch", 6)) {
        return T_CONTROL_FLOW_SWITCH;
      }
      if (mem_eq(start, "double", 6) || mem_eq(start, "struct", 6)) {
        return T_TYPE_STRUCT;
      }
      if (mem_eq(start, "extern", 6) || mem_eq(start, "inline", 6) ||
          mem_eq(start, "signed", 6) || mem_eq(start, "static", 6)){
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
      if (mem_eq(start, "register", 8) || mem_eq(start, "restrict", 8) ||
          mem_eq(start, "unsigned", 8) || mem_eq(start, "volatile", 8)){
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

#define TOKEN1(token) token_type = token
#define TOKEN2(token) ++token_end;    TOKEN1(token)
#define TOKEN3(token) token_end += 2; TOKEN1(token)

/** Parse the given source code into the `markers` array,
 * appending the new markers to whatever was already there.
 *
 *  The file must contain `#pragma Cedro x.y`
 * with `x` as the major and `y` as the minor revision.
 *  Everything up to that marker is output verbatim without any processing,
 * so standard C files are by default not modified.
 *
 *  Remember to empty the `markers` array before calling this function
 * if you are re-parsing from scratch.
 */
static Byte_p
parse(Byte_array_p src, mut_Marker_array_p markers)
{
  assert(PADDING_Byte_ARRAY >= 8); // Must be greater than the longest keyword.
  Byte_mut_p cursor = Byte_array_start(src);
  Byte_p     end    = Byte_array_end(src);
  Byte_mut_p prev_cursor = NULL;
  bool previous_token_is_value = false;

  error.len = 0;
  while (cursor is_not end) {
    assert(cursor is_not prev_cursor);
    prev_cursor = cursor;

    Byte_mut_p token_end = NULL;
    if        ((token_end = preprocessor(cursor, end))) {
      if (CEDRO_PRAGMA_LEN < (size_t)(token_end - cursor)) {
        if (mem_eq((Byte_p)CEDRO_PRAGMA, cursor, CEDRO_PRAGMA_LEN)) {
          mut_Marker inert;
          init_Marker(&inert, Byte_array_start(src), cursor, src, T_NONE);
          push_Marker_array(markers, inert);
          cursor = token_end;
          // Skip LF and empty lines after line.
          while (cursor is_not end and '\n' == *cursor) ++cursor;
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
    if (error.len) {
      eprintln("Error: src[%lu]: %s",
               cursor-Byte_array_start(src), as_c_string(&error));
      return cursor;
    }
    cursor = token_end;
  }
  if (markers->len is 0) {
    // No “#pragma Cedro x.y”, so just wrap the whole C code verbatim.
    mut_Marker inert;
    init_Marker(&inert,
                Byte_array_start(src), Byte_array_end(src), src, T_NONE);
    push_Marker_array(markers, inert);
  }


  // If CEDRO_PRAGMA is not present, markers will be empty.

  error.len = 0;
  while (cursor is_not end) {
    assert(cursor is_not prev_cursor);
    prev_cursor = cursor;

    mut_TokenType token_type = T_NONE;
    Byte_mut_p token_end = NULL;
    if        ((token_end = preprocessor(cursor, end))) {
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
      if (token_end is cursor) eprintln("error T_IDENTIFIER");
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
          if (c2 is '.' && c3 is '.') { TOKEN3(T_ELLIPSIS); }
          else                        { TOKEN1(T_OP_1);     }
          break;
        case '~': TOKEN1(T_OP_2);        break;
        case '?': case ':': TOKEN1(T_OP_13); break;
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
            case '=': TOKEN2(T_OP_14); break;
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
        default: TOKEN1(T_OTHER);
      }
    }
    if (error.len) {
      eprintln("Error: src[%lu]: %s",
               cursor-Byte_array_start(src), as_c_string(&error));
      return cursor;
    }
    mut_Marker marker;
    init_Marker(&marker, cursor, token_end, src, token_type);
    push_Marker_array(markers, marker);
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

/** Skip forward all `T_SPACE` and `T_COMMENT` markers. */
#define skip_space_forward(start, end) while (start is_not end and (start->token_type is T_SPACE or start->token_type is T_COMMENT)) ++start

/** Skip backward all `T_SPACE` and `T_COMMENT` markers. */
#define skip_space_back(start, end) while (end is_not start and ((end-1)->token_type is T_SPACE or (end-1)->token_type is T_COMMENT)) --end

/** starting at `cursor`, which should point to an
 * opening fence `{`, `[` or `(`, advance until the corresponding
 * closing fence `}`, `]` or `)` is found, then return that address.
 *  If the fences are not closed, the return value is `ènd`
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
  } while (matching_close is_not end and nesting is_not 0);

  if (nesting is_not 0 or matching_close is end) {
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
  Marker_mut_p start_of_line = cursor;
  size_t nesting = 0;
  while (start_of_line is_not start) {
    switch (start_of_line->token_type) {
      case T_SEMICOLON: case T_BLOCK_START: case T_BLOCK_END:
      case T_PREPROCESSOR:
        if (nesting is 0) {
          ++start_of_line;
          goto found;
        }
        break;
      case T_TUPLE_START: case T_INDEX_START:
        if (nesting is 0) {
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
    --start_of_line;
  }
found:

  if (nesting is_not 0 or start_of_line < start) {
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
      case T_SEMICOLON:
        if (nesting is 0) goto found;
        break;
      case T_BLOCK_START: case T_TUPLE_START: case T_INDEX_START:
        ++nesting;
        break;
      case T_BLOCK_END: case T_TUPLE_END: case T_INDEX_END:
        if (nesting is 0) goto found;
        else              --nesting;
        break;
      default: break;
    }
    ++end_of_line;
  }
found:

  if (nesting is_not 0 or end_of_line is end) {
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
  Marker_mut_p start_of_block = cursor;
  size_t nesting = 0;
  while (start_of_block >= start) {
    switch (start_of_block->token_type) {
      case T_BLOCK_START:
        if (nesting is 0) {
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
    --start_of_block;
  }
found:

  if (nesting is_not 0 or start_of_block < start) {
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
        if (nesting is 0) goto found;
        else              --nesting;
        break;
      default: break;
    }
    ++end_of_block;
  }
found:

  if (nesting is_not 0 or end_of_block is end) {
    err->message = LANG("Bloque sin cerrar.", "Unclosed block.");
    err->position = cursor;
  }

  return end_of_block;
}

/***************** macros *****************/
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
benchmark(mut_Byte_array_p src_p, Options_p options)
{
  const size_t repetitions = 1000;
  time_t start, end;
  time(&start);

  mut_Marker_array markers;
  init_Marker_array(&markers, 8192);

  for (size_t i = repetitions + 1; i; --i) {
    delete_Marker_array(&markers, 0, markers.len);

    parse(src_p, &markers);

    if (options->apply_macros) {
      macro_backstitch(&markers, src_p);
      macro_defer(&markers, src_p);
    }
    fputc('.', stderr);
  }

  time(&end);
  destruct_Marker_array(&markers);
  return difftime(end, start) / (double) repetitions;
}


#ifndef USE_CEDRO_AS_LIBRARY

static const char* const
usage_es =
    "Uso: cedro [opciones] fichero.c [fichero2.c … ]\n"
    "  El resultado va a stdout, puede usarse sin fichero intermedio así:\n"
    " cedro fichero.c | cc -x c - -o fichero\n"
    "  Es lo que hace el programa cedrocc:\n"
    " cedrocc -o fichero fichero.c\n"
    "\n"
    "  --apply-macros     Aplica las macros: pespunte, diferido, etc. (implícito)\n"
    "  --escape-ucn       Encapsula los caracteres no-ASCII en identificadores.\n"
    "  --discard-comments Descarta los comentarios.\n"
    "  --discard-space    Descarta los espacios en blanco.\n"
    "  --print-markers    Imprime los marcadores.\n"
    "  --no-apply-macros     No aplica las macros.\n"
    "  --no-escape-ucn       No encapsula caracteres en identificadores. (implícito)\n"
    "  --no-discard-comments No descarta los comentarios. (implícito)\n"
    "  --no-discard-space    No descarta los espacios.    (implícito)\n"
    "  --no-print-markers    No imprime los marcadores.   (implícito)\n"
    "\n"
    "  --enable-core-dump    Activa el volcado de memoria al estrellarse."
    " (implícito)\n"
    "  --no-enable-core-dump No activa el volcado de memoria al estrellarse.\n"
    "  --benchmark        Realiza una medición de rendimiento.\n"
    "  --version          Muestra la versión: " CEDRO_VERSION "\n"
    "                     El «pragma» correspondiente es: #pragma Cedro "
    CEDRO_VERSION
    ;
static const char* const
usage_en =
    "Usage: cedro [options] file.c [file2.c … ]\n"
    "  The result goes to stdout, can be used without an intermediate file like this:\n"
    " cedro file.c | cc -x c - -o file\n"
    "  It is what the cedrocc program does:\n"
    " cedrocc -o file file.c\n"
    "\n"
    "  --apply-macros     Apply the macros: backstitch, defer, etc. (default)\n"
    "  --escape-ucn       Escape non-ASCII in identifiers as UCN.\n"
    "  --discard-comments Discards the comments.\n"
    "  --discard-space    Discards all whitespace.\n"
    "  --print-markers    Prints the markers.\n"
    "  --no-apply-macros     Do not apply the macros.\n"
    "  --no-escape-ucn       Do not escape non-ASCII in identifiers. (default)\n"
    "  --no-discard-comments Does not discard comments.   (default)\n"
    "  --no-discard-space    Does not discard whitespace. (default)\n"
    "  --no-print-markers    Does not print the markers.  (default)\n"
    "\n"
    "  --enable-core-dump    Enable core dump on crash.   (default)\n"
    "  --no-enable-core-dump Do not enable core dump on crash.\n"
    "  --benchmark        Run a performance benchmark.\n"
    "  --version          Show version: " CEDRO_VERSION "\n"
    "                     The corresponding “pragma” is: #pragma Cedro "
    CEDRO_VERSION
    ;

int main(int argc, char** argv)
{
  Options options = { // Remember to keep the usage strings updated.
    .apply_macros     = true,
    .escape_ucn       = false,
    .discard_comments = false,
    .discard_space    = false,
    .print_markers    = false,
  };

  bool enable_core_dump = true;
  bool run_benchmark    = false;

  for (int i = 1; i < argc; ++i) {
    char* arg = argv[i];
    if (arg[0] is '-') {
      bool flag_value = true;
      if (strn_eq("--no-", arg, 6)) flag_value = false;
      if (str_eq("--apply-macros", arg) ||
          str_eq("--not-apply-macros", arg)) {
        options.apply_macros = flag_value;
      } else if (str_eq("--escape-ucn", arg) ||
                 str_eq("--not-escape-ucn", arg)) {
        options.escape_ucn = flag_value;
      } else if (str_eq("--discard-comments", arg) ||
                 str_eq("--not-discard-comments", arg)) {
        options.discard_comments = flag_value;
      } else if (str_eq("--discard-space", arg) ||
                 str_eq("--not-discard-space", arg)) {
        options.discard_space = flag_value;
      } else if (str_eq("--output-ucn", arg) ||
                 str_eq("--not-output-ucn", arg)) {
        options.escape_ucn = flag_value;
      } else if (str_eq("--print-markers", arg) ||
                 str_eq("--not-print-markers", arg)) {
        options.print_markers = flag_value;
      } else if (str_eq("--enable-core-dump", arg) ||
                 str_eq("--not-enable-core-dump", arg)) {
        enable_core_dump = flag_value;
      } else if (str_eq("--benchmark", arg)) {
        run_benchmark = true;
      } else if (str_eq("--version", arg)) {
        fprintf(stderr, CEDRO_VERSION "\n");
      } else {
        eprintln(LANG(usage_es, usage_en));
        return str_eq("-h", arg) || str_eq("--help", arg)? 0: 1;
      }
    }
  }

  if (enable_core_dump) {
    struct rlimit core_limit = { RLIM_INFINITY, RLIM_INFINITY };
    assert(0 is setrlimit(RLIMIT_CORE, &core_limit));
  }

  if (run_benchmark) {
    options.print_markers = false;
    options.apply_macros  = false;
  }

  mut_Marker_array markers;
  init_Marker_array(&markers, 8192);

  for (int i = 1; i < argc; ++i) {
    char* arg = argv[i];
    if (arg[0] is '-') continue;

    mut_Byte_array src;
    read_file(&src, arg);
    if (not src.len) {
      fprintf(stdout, ";// File \"%s\"\n#error %s\n",
              arg, strerror(errno));
      break;
    }

    markers.len = 0;

    parse(&src, &markers);

    if (run_benchmark) {
      double t = benchmark(&src, &options);
      if (t < 1.0) eprintln("%.fms for %s", t * 1000.0, arg);
      else         eprintln("%.1fs for %s", t         , arg);
    } else {
      if (options.apply_macros) {
        Macro_p macro = macros;
        while (macro->name && macro->function) {
          eprintln(LANG("Ejecutando macro %s:", "Running macro %s:"),
                   macro->name);
          macro->function(&markers, &src);
          ++macro;
        }
      }

      if (options.print_markers) {
        print_markers(&markers, &src, 0, 0, options);
      } else {
        unparse(&markers, &src, options, stdout);
      }
    }

    fflush(stdout);
    destruct_Marker_array(&markers);
    destruct_Byte_array(&src);
  }

  return 0;
}

#endif // USE_CEDRO_AS_LIBRARY
