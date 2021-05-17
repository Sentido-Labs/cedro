/* -*- coding: utf-8 c-basic-offset: 2 tab-width: 2 indent-tabs-mode: nil -*- */
/** \file */
/** \mainpage
 * The Cedro C pre-processor has three features:
 * the *backstitch* operator,
 * a simple resource release *defer* functionality,
 * and *block* (or multi-line) macros.
 *
 * For usage instructions, see [README.html](../../README.html).
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

#define log(...) fprintf(stderr, __VA_ARGS__), fputc('\n', stderr)

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
  /** Control flow keyword:
      `case, continue, default, else, if`.            */ T_CONTROL_FLOW,
  /** Control flow keyword: `do, for, while`.         */ T_CONTROL_FLOW_LOOP,
  /** Control flow keyword: `switch`.                 */ T_CONTROL_FLOW_SWITCH,
  /** Control flow keyword: `break`.                  */ T_CONTROL_FLOW_BREAK,
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
TokenType_STRING[] = {
  B("NONE"),
  B("Identifier"),
  B("Type"), B("Type struct"), B("Type qualifier"), B("Type qualifier auto"),
  B("Type definition"),
  B("Control flow"),
  B("Control flow loop"), B("Control flow switch"),
  B("Control flow break"), B("Control flow return"),
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
    size_t len;               /**< Length, in bytes/chars. */
    mut_TokenType token_type; /**< Token type. */
  });

/** Error while processing markers. */
TYPEDEF_STRUCT(Error, {
    Marker_mut_p position; /**< Position at which the problem was noticed. */
    const char * message;  /**< Message for user. */
  });

/** Initialize a marker with the given values. */
static void
init_Marker(mut_Marker_p _, size_t start, size_t end, TokenType token_type)
{
  _->start      = start;
  _->len        = end - start;
  _->token_type = token_type;
}

#include "array.h"

DEFINE_ARRAY_OF(Marker, 0, {});
DEFINE_ARRAY_OF(TokenType, 0, {});

TYPEDEF(Byte, uint8_t);
/** Add 8 bytes after end of buffer to avoid bounds checking while scanning
 * for tokens. No literal token is longer. */
DEFINE_ARRAY_OF(Byte, 8, {});

/* Lexer definitions. */

/** Match an identifier. */
static Byte_p
identifier(Byte_p start, Byte_p end)
{
  if (end <= start) return NULL;
  Byte_mut_p cursor = start;
  mut_Byte c = *cursor;
  if (in('a',c,'z') or in('A',c,'Z') or c is '_') {
    ++cursor;
    while (cursor < end) {
      c = *cursor;
      if (in('0',c,'9') or in('a',c,'z') or in('A',c,'Z') or c is '_') {
        ++cursor;
      } else break;
    }
    return cursor;
  } else {
    return NULL;
  }
}

/** Match a number.
 *  This matches invalid numbers like 3.4.6, 09, and 3e23.48.34e+11.
 *  Rejecting that is left to the compiler.
 */
static Byte_p
number(Byte_p start, Byte_p end)
{
  if (end <= start) return NULL;
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

/** Match a string literal. */
static Byte_p
string(Byte_p start, Byte_p end)
{
  if (end <= start or *start is_not '"') return NULL;
  Byte_mut_p cursor = start + 1;
  while (cursor is_not end and *cursor is_not '"') {
    if (*cursor is '\\' && cursor + 1 is_not end) ++cursor;
    ++cursor;
  }
  return (cursor is end)? NULL: cursor + 1;// End is past the closing symbol.
}

/** Match a character literal. */
static Byte_p
character(Byte_p start, Byte_p end)
{
  if (end <= start or *start is_not '\'') return NULL;
  Byte_mut_p cursor = start + 1;
  while (cursor is_not end and *cursor is_not '\'') {
    if (*cursor is '\\' && cursor + 1 is_not end) ++cursor;
    ++cursor;
  }
  return (cursor is end)? NULL: cursor + 1;// End is past the closing symbol.
}

/** Match whitespace: one or more space, `TAB`, `CR`, or `NL` characters. */
static Byte_p
space(Byte_p start, Byte_p end)
{
  if (end <= start) return NULL;
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

/** Match a comment block. */
static Byte_p
comment(Byte_p start, Byte_p end)
{
  if (end <= start + 1 or *start is_not '/') return NULL;
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

/** Match a pre-processor directive. */
static Byte_p
preprocessor(Byte_p start, Byte_p end)
{
  if (end is start or *start is_not '#') return NULL;
  Byte_mut_p cursor = start;
  do {
    cursor = memchr(cursor + 1, '\n', (size_t)(end - cursor));
    if (!cursor) { cursor = end; break; }
  } while ('\\' == *(cursor - 1));
  return cursor; // The newline is not part of the token.
}



/** Extract the indentation of the line for the character at `index`,
 * including the preceding `LF` character if it exists.
 */
static Marker
indentation(Byte_array_p src, size_t index)
{
  // TODO: change signature to (Byte_array_p src, Marker_array_p markers, size_t index)
  // Then index will be a marker index, not a byte index.
  Byte_p start = Byte_array_start(src);
  Byte_p end   = Byte_array_end(src);
  Byte_mut_p start_of_line = get_Byte_array(src, index);
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
  while (end_of_indentation is_not end) {
    if (*end_of_indentation is_not ' ' &&
        *end_of_indentation is_not '\t') break;
    ++end_of_indentation;
  }
  mut_Marker indentation = {
    .start = (size_t)(start_of_line - start),
    .len   = (size_t)(end_of_indentation - start_of_line),
    .token_type = T_SPACE
  };
  return indentation;
}

/** Get new slice for the given marker.
 */
static Byte_array_slice
slice_for_marker(Byte_array_p src, Marker_p cursor)
{
  Byte_array_slice slice = {
    .start_p = get_Byte_array(src, cursor->start),
    .end_p   = get_Byte_array(src, cursor->start + cursor->len)
  };
  return slice;
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

/** Compute the number of LF characters in the given marker segment.
 *
 *  To get the line number at `position`, use `line_number()`.
 */
static size_t
count_line_ends_between(Byte_array_p src, Marker_p start, Marker_p marker_p)
{
  size_t count = 0;
  Marker_mut_p cursor = marker_p;
  while (cursor is_not start) {
    Byte_array_slice src_segment = slice_for_marker(src, cursor);
    Byte_mut_p pointer = src_segment.start_p;
    while ((pointer =
            memchr(pointer, '\n', (size_t)(src_segment.end_p - pointer) ))) {
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
  return 1 + count_line_ends_between(src,
                                     Marker_array_start(markers),
                                     position
                                     );
}

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

/** Indent by the given number of double spaces, for the next `log()`
 * or `fprintf(stder, …)`. */
static void
indent_log(size_t indent)
{
  if (indent > 40) indent = 40;
  while (indent-- is_not 0) fprintf(stderr, "  ");
}
/** Log to `stderr` with the given indentation. */
#define log_indent(indent, ...) { indent_log(indent); log(__VA_ARGS__); }

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
    for (; (cursor = memchr(cursor, first_character, src->len)); ++cursor) {
      Byte_mut_p p1 = cursor;
      Byte_mut_p p2 = (Byte_p) text;
      while (*p2 && p1 is_not end && *p1 is *p2) { ++p1; ++p2; }
      if (*p2 is 0) {
        match = cursor;
        break;
      }
    }
  }
  mut_Marker marker = { .start = 0, .len = text_len, .token_type = token_type };
  if (match) {
    marker.start = (size_t)(match - Byte_array_start(src));
  } else {
    marker.start = src->len;
    Byte_mut_p p2 = (Byte_p) text;
    while (*p2) push_Byte_array(src, *(p2++));
  }

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
        log_indent(indent++,
                   "%s %s← %s", token, spc, TokenType_STRING[m->token_type]);
        break;
      case T_BLOCK_END: case T_TUPLE_END: case T_INDEX_END:
        log_indent(--indent,
                   "%s %s← %s", token, spc, TokenType_STRING[m->token_type]);
        break;
      case T_GROUP_START: case T_GROUP_END:
        /* Invisible grouping of tokens. */
        break;
      case T_SPACE:
        if (not options.discard_space) {
          log_indent(indent, "%s %s← Space", token, spc);
        }
        break;
      case T_COMMENT:
        if (not options.discard_comments) {
          log_indent(indent, "%s %s← Comment", token, spc);
        }
        break;
      default:
        log_indent(indent,
                   "%s %s← %s", token, spc, TokenType_STRING[m->token_type]);
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
    Byte_mut_p text = src->items + m->start;
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
        fwrite(text, sizeof(*src->items), m->len - len, out);
        for (++m; m is_not m_end; ++m) {
          if (options.discard_comments && m->token_type is T_COMMENT) {
            continue;
          } else if (m->token_type is T_PREPROCESSOR) {
            text = src->items + m->start;
            if (strn_eq("#define }", (char*)text, m->len < 9? m->len: 9)) {
              puts("// End #define");
              break;
            }
            fwrite(text, sizeof(*src->items), m->len, out);
          } else {
            text = src->items + m->start;
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
              fwrite(text, sizeof(*src->items), (size_t)(eol - text), out);
              if (is_line_comment) {
                fputs(" */", out);
                is_line_comment = false;
              }
              fputs(" \\\n", out);
              len -= (size_t)(eol - text) + 1;
              text = eol + 1;
            }
            fwrite(text, sizeof(*src->items), len, out);
            if (is_line_comment) {
              fputs(" */", out);
            }
          }
        }
        continue;
      }
    }
    fwrite(text, sizeof(*src->items), m->len, out);
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
        return T_CONTROL_FLOW;
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
      if (mem_eq(start, "case", 4) || mem_eq(start, "else", 4)) {
        return T_CONTROL_FLOW;
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
        return T_CONTROL_FLOW;
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
        return T_CONTROL_FLOW;
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
 *  Remember to empty the `markers` array before calling this function
 * if you are re-parsing from scratch.
 */
static Byte_p
parse(Byte_array_p src, mut_Marker_array_p markers)
{
  assert(PADDING_Byte_ARRAY >= 8); // Must be greater than the longest keyword.
  Byte_mut_p cursor = src->items;
  Byte_p end = src->items + src->len;
  Byte_mut_p prev_cursor = NULL;
  bool previous_token_is_value = false;
  while (cursor is_not end) {
    if (cursor is prev_cursor) {
      log("ERROR: endless loop after %ld.\n", cursor - src->items);
      break;
    }
    prev_cursor = cursor;

    mut_TokenType token_type = T_NONE;
    Byte_mut_p token_end = NULL;
    if        ((token_end = identifier(cursor, end))) {
      TOKEN1(keyword_or_identifier(cursor, token_end));
      if (token_end is cursor) log("error T_IDENTIFIER");
    } else if ((token_end = string(cursor, end))) {
      TOKEN1(T_STRING);
      if (token_end is cursor) { log("error T_STRING"); break; }
    } else if ((token_end = number(cursor, end))) {
      TOKEN1(T_NUMBER);
      if (token_end is cursor) { log("error T_NUMBER"); break; }
    } else if ((token_end = character(cursor, end))) {
      TOKEN1(T_CHARACTER);
      if (token_end is cursor) { log("error T_CHARACTER"); break; }
    } else if ((token_end = comment(cursor, end))) {
      TOKEN1(T_COMMENT);
      if (token_end is cursor) { log("error T_COMMENT"); break; }
    } else if ((token_end = space(cursor, end))) {
      TOKEN1(T_SPACE);
      if (token_end is cursor) { log("error T_SPACE"); break; }
    } else if ((token_end = preprocessor(cursor, end))) {
      TOKEN1(T_PREPROCESSOR);
      if (token_end is cursor) { log("error T_PREPROCESSOR"); break; }
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
    mut_Marker marker;
    init_Marker(&marker,
                (size_t)(cursor - src->items),
                (size_t)(token_end - src->items),
                token_type);
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
    err->message = "Unclosed group.";
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
    err->message = "Excess group closings.";
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
    err->message = "Unclosed group.";
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
    err->message = "Excess block closings.";
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
    err->message = "Unclosed block.";
    err->position = cursor;
  }

  return end_of_block;
}

typedef FILE* mut_File_p;
typedef char*const FilePath;
/** Read a file into the given buffer. Errors are printed to `stderr`. */
static void
read_file(mut_Byte_array_p _, FilePath path)
{
  mut_File_p input = fopen(path, "r");
  if (!input) {
    fprintf(stderr, "File not found: %s\n", path);
    init_Byte_array(_, 0);
    return;
  }
  fseek(input, 0, SEEK_END);
  size_t size = (size_t)ftell(input);
  init_Byte_array(_, size);
  rewind(input);
  _->len = fread((mut_Byte_p) _->items, sizeof(*_->items), size, input);
  if (feof(input)) {
    fprintf(stderr, "Unexpected EOF at %ld reading “%s”.\n", _->len, path);
  } else if (ferror(input)) {
    fprintf(stderr, "Error reading “%s”.\n", path);
  } else {
    memset((mut_Byte_p) _->items + _->len, 0,
           (_->capacity - _->len) * sizeof(*_->items));
  }
  fclose(input); input = NULL;
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
  const size_t repetitions = 100;
  time_t start, end;
  time(&start);

  mut_Marker_array markers;
  init_Marker_array(&markers, 8192);

  for (size_t i = repetitions + 1; i; --i) {
    delete_Marker_array(&markers, 0, markers.len);

    parse(src_p, &markers);

    if (options->apply_macros) {
      /*log("Running macro count_markers:");
      macro_count_markers(&markers, src_p);
      log("Running macro collect_typedefs:");
      macro_collect_typedefs(&markers, src_p);
      log("Running macro fn:");
      macro_fn(&markers, src_p);
      log("Running macro let:");
      macro_let(&markers, src_p);*/
      //log("Running macro backstitch:");
      macro_backstitch(&markers, src_p);
    }
    fputc('.', stderr);
  }

  time(&end);
  destruct_Marker_array(&markers);
  return difftime(end, start) / (double) repetitions;
}

static const char* const
usage_es =
    "Uso: cedro [opciones] fichero.c [fichero2.c … ]\n"
    "  El resultado va a stdout, puede usarse sin fichero intermedio así:\n"
    " cedro fichero.c | gcc -x c - -o fichero\n"
    "  Es lo que hace el programa cedrocc:\n"
    " cedrocc -o fichero fichero.c\n"
    "\n"
    "  --discard-space    Descarta los espacios en blanco.\n"
    "  --discard-comments Descarta los comentarios.\n"
    "  --print-markers    Imprime los marcadores.\n"
    "  --no-discard-space    No descarta los espacios.    (implícito)\n"
    "  --no-discard-comments No descarta los comentarios. (implícito)\n"
    "  --no-print-markers    No imprime los marcadores.   (implícito)\n"
    "\n"
    "  --enable-core-dump    Activa el volcado de memoria al estrellarse."
    " (implícito)\n"
    "  --no-enable-core-dump No activa el volcado de memoria al estrellarse.\n"
    "  --benchmark        Realiza una medición de rendimiento.\n"
    "  --version          Muestra la versión: " CEDRO_VERSION
    ;
static const char* const
usage_en =
    "Usage: cedro [options] file.c [file2.c … ]\n"
    "  The result goes to stdout, can be used without an intermediate file like this:\n"
    " cedro file.c | gcc -x c - -o file\n"
    "  It is what the cedrocc program does:\n"
    " cedrocc -o fichero fichero.c\n"
    "\n"
    "  --discard-space    Discards all whitespace.\n"
    "  --discard-comments Discards the comments.\n"
    "  --print-markers    Prints the markers.\n"
    "  --no-discard-space    Does not discard whitespace. (default)\n"
    "  --no-discard-comments Does not discard comments.   (default)\n"
    "  --no-print-markers    Does not print the markers.  (default)\n"
    "\n"
    "  --enable-core-dump    Enable core dump on crash.   (default)\n"
    "  --no-enable-core-dump Do not enable core dump on crash.\n"
    "  --benchmark        Run a performance benchmark.\n"
    "  --version          Show version: " CEDRO_VERSION
    ;

int main(int argc, char** argv)
{
  Options options = { // Remember to keep the usage strings updated.
    .discard_comments = false,
    .discard_space    = false,
    .apply_macros     = true,
    .print_markers    = false,
  };

  bool enable_core_dump = true;
  bool run_benchmark    = false;

  for (int i = 1; i < argc; ++i) {
    char* arg = argv[i];
    if (arg[0] is '-') {
      bool flag_value = true;
      if (strn_eq("--no-", arg, 6)) flag_value = false;
      if (str_eq("--discard-comments", arg) ||
          str_eq("--not-discard-comments", arg)) {
        options.discard_comments = flag_value;
      } else if (str_eq("--discard-space", arg) ||
                 str_eq("--not-discard-space", arg)) {
        options.discard_space = flag_value;
      } else if (str_eq("--apply-macros", arg) ||
                 str_eq("--not-apply-macros", arg)) {
        options.apply_macros = flag_value;
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
        log(strn_eq(getenv("LANG"), "es", 2)?
            usage_es:
            usage_en);
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

    markers.len = 0;

    parse(&src, &markers);

    if (run_benchmark) {
      double t = benchmark(&src, &options);
      if (t < 1.0) log("%.fms for %s", t * 1000.0, arg);
      else         log("%.1fs for %s", t         , arg);
    } else {
      if (options.apply_macros) {
        Macro_p macro = macros;
        while (macro->name && macro->function) {
          log("Running macro %s:", macro->name);
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
