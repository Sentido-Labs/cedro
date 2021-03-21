/* -*- coding: utf-8 c-basic-offset: 2 tab-width: 2 indent-tabs-mode: nil -*- */
/** \file */
/**
 * \mainpage
 * The Cedro C pre-processor.
 *   - [Data structures](cedro_8c.html#nested-classes)
 *   - [Macros](cedro_8c.html#define-members)
 *   - [Typedefs](cedro_8c.html#typedef-members)
 *   - [Enumerations](cedro_8c.html#enum-members)
 *   - [Functions](cedro_8c.html#func-members)
 *   - [Variables](cedro_8c.html#var-members)
 *
 * \author Alberto González Palomo https://sentido-labs.com
 * \copyright ©2021 Alberto González Palomo https://sentido-labs.com
 *
 * Created: 2020-11-25 22:41
 */
#define CEDRO_VERSION "1.0"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <iso646.h>
#define is_not not_eq
#define is     ==
#define in(min, x, max) (x >= min && x <= max)
#include <string.h> // For memcpy(), memmove().
#define mem_eq(start, bytes, len) (0 == memcmp(start, bytes, len))
#define str_eq(a, b)              (0 == strcmp(a, b))

#include <assert.h>
#include <sys/resource.h>

#define log(...) fprintf(stderr, __VA_ARGS__), fputc('\n', stderr)

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

/// Binary string, `const unsigned char const*`.
#define B(string) ((const unsigned char const*)string)

/**
   These token types loosely correspond to those in the C grammar.

   For the operator precedence levels, see:
   https://en.cppreference.com/w/c/language/operator_precedence */
typedef enum TokenType {
  /** No token, used as marker for uninitialized data. */ T_NONE,
  /** Identifier.                                    */ T_IDENTIFIER,
  /** Type name.                                     */ T_TYPE,
  /** Type struct.                                   */ T_TYPE_STRUCT,
  /** Type qualifier.                                */ T_TYPE_QUALIFIER,
  /** Type definition.                               */ T_TYPEDEF,
  /** Control flow keyword.                          */ T_CONTROL_FLOW,
  /** Number, either integer or float.               */ T_NUMBER,
  /** String including the quotes: `"ABC"`           */ T_STRING,
  /** Character including the apostrophes: ```'A'``` */ T_CHARACTER,
  /** Whitespace, a block of either `SP`, `HT`, `LF` or `CR`.
      See [Wikipedia, Basic ASCII control codes](https://en.wikipedia.org/wiki/C0_and_C1_control_codes#Basic_ASCII_control_codes)    */ T_SPACE,
  /** Comment block or line.                         */ T_COMMENT,
  /** Preprocessor directive.                        */ T_PREPROCESSOR,
  /** Generic macro.                                 */ T_GENERIC_MACRO,
  /** Start of a block: `{`                          */ T_BLOCK_START,
  /** End of a block: `}`                            */ T_BLOCK_END,
  /** Start of a tuple: `(`                          */ T_TUPLE_START,
  /** End of a tuple: `)`                            */ T_TUPLE_END,
  /** Start of an array index: `[`                   */ T_INDEX_START,
  /** End of an array index: `]`                     */ T_INDEX_END,
  /** Invisible grouping of tokens, for instance for operator precedence.
                                                     */ T_GROUP_START,
  /** End invisible grouping of tokens.              */ T_GROUP_END,
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
} TokenType;
static const unsigned char * const TOKEN_NAME[] = {
  B("NONE"),
  B("Identifier"),
  B("Type"), B("Type struct"), B("Type qualifier"), B("Type definition"),
  B("Control flow"),
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

/** Defines mutable types mut_〈T〉 and mut_〈T〉_p (pointer),
    and constant types 〈T〉 and 〈T〉_p (pointer to constant).

    You can define similar types for an existing type, for instance `uint8_t`:
    ```
    typedef       uint8_t mut_Byte, * const mut_Byte_p, *  mut_Byte_mut_p;
    typedef const uint8_t     Byte, * const     Byte_p, *      Byte_mut_p;
    ```
*/
#define TYPEDEF(T, STRUCT)                                                    \
  typedef struct T STRUCT mut_##T, * mut_##T##_mut_p, * const mut_##T##_p;    \
  typedef const struct T        T, * const T##_p,     *             T##_mut_p

/** Marks a C token in the source code. */
TYPEDEF(Marker, {
    size_t start;         /**< Start position, in bytes/chars. */
    size_t len;           /**< Length, in bytes/chars. */
    TokenType token_type; /**< Token type. */
  });

/** Error while processing markers. */
TYPEDEF(Error, {
    size_t position;      /**< Position at which the problem was noticed. */
    const char * message; /**< Message for user. */
  });

/** Initialize a marker with the given values. */
void init_Marker(mut_Marker_p _, size_t start, size_t end, TokenType token_type)
{
  _->start      = start;
  _->len        = end - start;
  _->token_type = token_type;
}

#define CONST_AND_MUT_VARIANT(T, NAME) T NAME
/* First GCC supporting this is 4.6. https://gcc.gnu.org/wiki/C11Status
   With C11 we could have const/mut variants:
#define CONST_AND_MUT_VARIANT(T, NAME) union { T NAME; mut_##T mut_##NAME; }
 */

/** DESTRUCT_BLOCK is a block of code that releases the resources for a block
    of objects of type T, between `mut_##T##_p cursor` and `T##_p end`.\n
    For instance: { while (cursor != end) destruct_##T(cursor++); } \n
    If the type does not need any clean-up, just use `{}`.
*/
#define DEFINE_ARRAY_OF(T, PADDING, DESTRUCT_BLOCK)                     \
  /** The constant `PADDING_##T##_ARRAY` = `PADDING`                    \
      defines how many items are physically available                   \
      after those valid elements.                                     \n\
      This can be used to avoid special cases near the end              \
      when searching for fixed-length sequences in the array,           \
      although you have to set them to `0` or other appropriate value.  \
   */                                                                   \
typedef struct T##_array {                                              \
  /** Current length, the number of valid elements. */                  \
  size_t len;                                                           \
  /** Maximum length before reallocation is needed. */                  \
  size_t capacity;                                                      \
  /** The items stored in this array. */                                \
  CONST_AND_MUT_VARIANT(T##_mut_p, items);                              \
} mut_##T##_array, * const mut_##T##_array_p, * mut_##T##_array_mut_p;  \
typedef const struct T##_array                                          \
/*   */ T##_array, * const       T##_array_p, *       T##_array_mut_p;  \
                                                                        \
/** Example:                                                            \
    <code>T##_array_slice s = { &items, &items + 10 };</code> */        \
typedef struct T##_array_slice {                                        \
  /** Start address. */                                                 \
  T##_mut_p start_p;                                                    \
  /** End addres. */                                                    \
  T##_mut_p end_p;                                                      \
} mut_##T##_array_slice, * const mut_##T##_array_slice_p,               \
  /**/                   *       mut_##T##_array_slice_mut_p;           \
typedef const struct T##_array_slice                                    \
/*   */ T##_array_slice, * const T##_array_slice_p,                     \
  /**/                   *       T##_array_slice_mut_p;                 \
                                                                        \
/**                                                                     \
   A slice of an array where the elements are <b>mut</b>able.           \
                                                                        \
   Example:                                                             \
   <code>T##_mut_array a;</code>                                        \
   <code>init_##T##_array(&a, 0);</code>                                \
   <code>T##_mut_array_slice s;</code>                                  \
   <code>init_##T##_array_slice(&s, &a, 3, 10);</code>                  \
   <code>assert(&s->start_p == &a->items + 3;</code>                    \
   <code>assert(&s->end_p   == &a->items + 10;</code>                   \
*/                                                                      \
typedef struct T##_array_mut_slice {                                    \
  /** Start address. */                                                 \
  mut_##T##_mut_p start_p;                                              \
  /** End addres. */                                                    \
  mut_##T##_mut_p end_p;                                                \
} /* Mutable slice of a mutable array whose items are read-only.        \
   */     mut_##T##_array_mut_slice,                                    \
  /* Pointer to mutable slice of a mutable array of read-only items. */ \
  * const mut_##T##_array_mut_slice_p,                                  \
  /* Mutable pointer to mutable slice of mutable read-only array.*/     \
  *       mut_##T##_array_mut_slice_mut_p;                              \
/**                                                                     \
   A slice of an array where the elements are <b>const</b>ants.         \
                                                                        \
   Example:                                                             \
   <code>T##_mut_array a;</code>                                        \
   <code>init_##T##_array(&a, 0);</code>                                \
   <code>T##_mut_array_slice s;</code>                                  \
   <code>init_##T##_array_slice(&s, &a, 3, 10);</code>                  \
   <code>assert(&s->start_p == &a->items + 3;</code>                    \
   <code>assert(&s->end_p   == &a->items + 10;</code>                   \
*/                                                                      \
typedef const struct T##_array_mut_slice                                \
/* Slice of a mutable array whose items are read-only.                  \
 */             T##_array_mut_slice,                                    \
  /* Pointer to mutable slice of a mutable array of read-only items. */ \
  * const       T##_array_mut_slice_p,                                  \
  /* Mutable pointer to mutable slice of mutable read-only array.*/     \
  *             T##_array_mut_slice_mut_p;                              \
                                                                        \
/** Initialize the slice to point at (*array_p)[`start`...`end`].       \
    `end` can be 0, in which case the slice extends to                  \
    the end of `array_p`. */                                            \
void                                                                    \
init_##T##_array_slice(mut_##T##_array_slice_p _,                       \
                       T##_array_p array_p,                             \
                       size_t start, size_t end)                        \
{                                                                       \
  _->start_p = array_p->items + start;                                  \
  if (end) _->end_p = array_p->items + end;                             \
  else     _->end_p = array_p->items + array_p->len;                    \
}                                                                       \
                                                                        \
/** Initialize the slice to point at (*array_p)[`start`...`end`].       \
    `end` can be 0, in which case the slice extends to                  \
    the end of `array_p`. */                                            \
void                                                                    \
init_##T##_array_mut_slice(mut_##T##_array_mut_slice_p _,               \
                           mut_##T##_array_mut_p array_p,               \
                           size_t start, size_t end)                    \
{                                                                       \
  _->start_p = (mut_##T##_mut_p) array_p->items + start;                \
  if (end) _->end_p = (mut_##T##_mut_p) array_p->items + end;           \
  else     _->end_p = (mut_##T##_mut_p) array_p->items + array_p->len;  \
}                                                                       \
                                                                        \
void destruct_##T##_block(mut_##T##_p cursor, T##_p end)                \
{                                                                       \
  DESTRUCT_BLOCK                                                        \
}                                                                       \
/** Initialize the array at the given pointer.                        \n\
    For local variables, use it like this:                            \n\
    \code{.c}                                                           \
    mut_##T##_array things;                                             \
    init_##T##_array(&things, 100); ///< We expect around 100 items.    \
    {...}                                                               \
    destruct_##T##_array(&things);                                      \
    \endcode                                                            \
 */                                                                     \
void                                                                    \
init_##T##_array(mut_##T##_array_p _, size_t initial_capacity)          \
{                                                                       \
  _->len = 0;                                                           \
  _->capacity = initial_capacity + PADDING;                             \
  _->items = malloc(_->capacity * sizeof *_->items);                    \
  /* Used malloc() here instead of calloc() because we need realloc()   \
     later anyway, so better keep the exact same behaviour. */          \
}                                                                       \
/** Release any resources allocated for this struct. */                 \
void                                                                    \
destruct_##T##_array(mut_##T##_array_p _)                               \
{                                                                       \
  destruct_##T##_block((mut_##T##_p) _->items, _->items + _->len);      \
  _->len = 0;                                                           \
  _->capacity = 0;                                                      \
  free((mut_##T##_mut_p) (_->items));                                   \
  *((mut_##T##_mut_p *) &(_->items)) = NULL;                            \
}                                                                       \
/** Abandon any resources allocated for this struct.                    \
    This just indicates that we transferred ownership somewhere else.   \
 */                                                                     \
static void                                                             \
abandon_##T##_array(mut_##T##_array_p _)                                \
{                                                                       \
  _->len = 0;                                                           \
  _->capacity = 0;                                                      \
  *((mut_##T##_mut_p *) &(_->items)) = NULL;                            \
}                                                                       \
                                                                        \
/** Push a bit copy of the element on the end/top of the array,         \
    resizing the array if needed. */                                    \
void                                                                    \
push_##T##_array(mut_##T##_array_p _, T##_p item)                       \
{                                                                       \
  if (_->capacity < _->len + 1 + PADDING) {                             \
    _->capacity = 2*_->capacity + PADDING;                              \
    _->items = realloc((void*) _->items,                                \
                       _->capacity * sizeof *_->items);                 \
  }                                                                     \
  *((mut_##T##_mut_p) _->items + _->len++) = *item;                     \
}                                                                       \
                                                                        \
/** Splice the given slice in place of the removed elements,            \
    resizing the array if needed.                                       \
    Starting at `position`, `delete` elements are deleted and           \
    the elements in the `insert` slice are inserted there               \
    as bit copies.                                                      \
    The `insert` slice, if given, must belong to a different array. */  \
void                                                                    \
splice_##T##_array(mut_##T##_array_p _,                                 \
                   size_t position, size_t delete,                      \
                   T##_array_slice_p insert)                            \
{                                                                       \
  assert(position + delete <= _->len);                                  \
  destruct_##T##_block((mut_##T##_p) _->items + position,               \
                   _->items + position + delete);                       \
                                                                        \
  size_t insert_len = 0;                                                \
  if (insert) {                                                         \
    assert(_->items          > insert->end_p ||                         \
           _->items + _->len < insert->start_p);                        \
    assert(insert->end_p >= insert->start_p);                           \
    insert_len = insert->end_p - insert->start_p;                       \
    size_t new_len = _->len + insert_len - delete;                      \
    if (new_len >= _->capacity) {                                       \
      while (new_len >= _->capacity) _->capacity = 2*_->capacity;       \
      _->capacity += PADDING;                                           \
      _->items = realloc((void*) _->items,                              \
                         _->capacity * sizeof *_->items);               \
    }                                                                   \
  }                                                                     \
                                                                        \
  size_t gap_end = position + insert_len;                               \
  memmove((void*) (_->items + gap_end),                                 \
          _->items + position + delete,                                 \
          (_->len - delete - position) * sizeof *_->items);             \
  _->len = _->len + insert_len - delete;                                \
  if (insert_len) {                                                     \
    memcpy((void*) (_->items + position),                               \
           insert->start_p,                                             \
           insert_len * sizeof *_->items);                              \
  }                                                                     \
}                                                                       \
                                                                        \
/** Delete `delete` elements from the array at `position`. */           \
void                                                                    \
delete_##T##_array(mut_##T##_array_p _, size_t position, size_t delete) \
{                                                                       \
  destruct_##T##_block((mut_##T##_p) _->items + position,               \
                   _->items + position + delete);                       \
                                                                        \
  memmove((void*) (_->items + position),                                \
          _->items + position + delete,                                 \
          (_->len - delete - position) * sizeof *_->items);             \
  _->len -= delete;                                                     \
}                                                                       \
                                                                        \
/** Remove the element at the end/top of the array,                     \
    copying its bits into `*item_p` unless `item_p` is `0`,             \
    in which case destruct_##T##_block()                                \
    is called on those elements. */                                     \
void                                                                    \
pop_##T##_array(mut_##T##_array_p _, mut_##T##_p item_p)                \
{                                                                       \
  if (not _->len) return;                                               \
  mut_##T##_p last_p = (mut_##T##_p) _->items + _->len - 1;             \
  if (item_p) memmove((void*) last_p, item_p, sizeof *_->items);        \
  else        destruct_##T##_block((mut_##T##_p) last_p, last_p + 1);   \
  --_->len;                                                             \
}                                                                       \
                                                                        \
/** Return a pointer to the element at `position`.                      \
    Panics if the index is out of range. */                             \
T##_p                                                                   \
get_##T##_array(T##_array_p _, size_t position)                         \
{                                                                       \
  assert(position < _->len);                                            \
  return _->items + position;                                           \
}                                                                       \
                                                                        \
/** Return a mutable pointer to the element at `position`.              \
    Panics if the index is out of range. */                             \
mut_##T##_p                                                             \
get_mut_##T##_array(T##_array_p _, size_t position)                     \
{                                                                       \
  assert(position < _->len);                                            \
  return (mut_##T##_p) _->items + position;                             \
}                                                                       \
                                                                        \
/** Return a pointer to the start of the array (same as `_->items`) */  \
T##_p                                                                   \
T##_array_start(T##_array_p _)                                          \
{                                                                       \
  return _->items;                                                      \
}                                                                       \
                                                                        \
/** Return a pointer to the next byte after                             \
    the element at the end of the array. */                             \
T##_p                                                                   \
T##_array_end(T##_array_p _)                                            \
{                                                                       \
  return _->items + _->len;                                             \
}                                                                       \
const size_t PADDING_##T##_ARRAY = PADDING//; commented out to avoid ;;.

DEFINE_ARRAY_OF(Marker, 0, {});

typedef uint8_t
/*   */ mut_Byte, * const mut_Byte_p, *  mut_Byte_mut_p;
typedef const uint8_t
/*   */     Byte, * const     Byte_p, *      Byte_mut_p;
/** Add 8 bytes after end of buffer to avoid bounds checking while scanning
 *  for tokens. No literal token is longer. */
DEFINE_ARRAY_OF(Byte, 8, {});
typedef mut_Byte_array mut_Buffer, *mut_Buffer_p;
typedef     Byte_array     Buffer, *    Buffer_p;
#define init_Buffer init_Byte_array
#define destruct_Buffer destruct_Byte_array
#define PADDING_Buffer PADDING_Byte_ARRAY

typedef Byte_p         SourceCode; /**< 0-terminated. */
typedef Byte_mut_p mut_SourceCode; /**< 0-terminated. */

/* Lexer definitions. */

/** Match an identifier. */
SourceCode identifier(SourceCode start, SourceCode end)
{
  if (end <= start) return NULL;
  mut_SourceCode cursor = start;
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
    This matches invalid numbers like 3.4.6, 09, and 3e23.48.34e+11.
    Rejecting that is left to the compiler. */
SourceCode number(SourceCode start, SourceCode end)
{
  if (end <= start) return NULL;
  mut_SourceCode cursor = start;
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
SourceCode string(SourceCode start, SourceCode end)
{
  if (*start is_not '"' or end <= start) return NULL;
  mut_SourceCode cursor = start + 1;
  while (cursor is_not end and *cursor is_not '"') {
    if (*cursor is '\\' && cursor + 1 is_not end) ++cursor;
    ++cursor;
  }
  return (cursor is end)? NULL: cursor + 1;// End is past the closing symbol.
}

/** Match an `#include <...>` directive. */
SourceCode system_include(SourceCode start, SourceCode end)
{
  if (*start is_not '<' or end <= start) return NULL;
  mut_SourceCode cursor = start + 1;
  while (cursor is_not end and *cursor is_not '>') ++cursor;
  return (cursor is end)? NULL: cursor + 1;// End is past the closing symbol.
}

/** Match a character literal. */
SourceCode character(SourceCode start, SourceCode end)
{
  if (*start is_not '\'' or end <= start) return NULL;
  mut_SourceCode cursor = start + 1;
  while (cursor is_not end and *cursor is_not '\'') {
    if (*cursor is '\\' && cursor + 1 is_not end) ++cursor;
    ++cursor;
  }
  return (cursor is end)? NULL: cursor + 1;// End is past the closing symbol.
}

/** Match whitespace: one or more space, TAB, CR, or NL characters. */
SourceCode space(SourceCode start, SourceCode end)
{
  if (end <= start) return NULL;
  mut_SourceCode cursor = start;
  while (cursor < end) {
    switch (*cursor) {
      case ' ': case '\t': case '\n': case '\r':
        ++cursor;
        break;
      case '\\':
        // No need to check bounds thanks to PADDING_Buffer.
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
SourceCode comment(SourceCode start, SourceCode end)
{
  if (*start is_not '/' or end <= start + 1) return NULL;
  mut_SourceCode cursor = start + 1;
  if (*cursor is '/') {
    ++cursor;
    while (cursor < end and *cursor is_not '\n') ++cursor;
    return cursor;// The newline is not part of the comment.
  }
  if (*cursor is_not '*') return NULL;
  while (cursor < end) {
    if (*cursor is '*') {
      ++cursor;
      if (cursor < end and *cursor is '/') break;
    }
    ++cursor;
  }
  return (cursor is end)? NULL: cursor + 1;// End is past the closing symbol.
}

/** Match a line comment. */
SourceCode comment_line(SourceCode start, SourceCode end)
{
  if (*start is_not '/' or end <= start + 1) return NULL;
  mut_SourceCode cursor = start + 1;
  if (*cursor is_not '/') return NULL;
  while (cursor < end && *cursor is_not '\n') ++cursor;
  return cursor;
}

/** Match a pre-processor directive. */
SourceCode preprocessor(SourceCode start, SourceCode end)
{
  if (*start is_not '#' or end <= start) return NULL;
  mut_SourceCode cursor = start + 1;
  if (cursor is end) return cursor;
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
  } else if (c is '#') {
    // Token concatenation.
    return cursor + 1;
  } else {
    return NULL;
  }
}



/** Compute the line number for the given position,
    which is a cursor position. */
size_t line_number(Buffer_p src, size_t position)
{
  size_t line_number = 1;
  mut_SourceCode pointer = src->items;

  while ((pointer =
          memchr(pointer, '\n', src->items + position - pointer ))) {
    ++pointer;
    ++line_number;
  }
  return line_number;
}

/** Extract the indentation of the line for the character at `index`,
    including the preceding `LF` character if it exists.
 */
Marker indentation(Buffer_p src, size_t index)
{
  Byte_mut_p start_of_line = get_Byte_array(src, index);
  Byte_p start = Byte_array_start(src);
  Byte_p end   = Byte_array_end(src);
  while (*start_of_line is_not '\n') {
    if (start_of_line is start) break;
    --start_of_line;
  }
  // If *start_of_line is not '\n' here, there is no LF to re-use.
  // This can only happen at the very fist line of the file,
  // which should have no indentation anyway.
  // Handling the hypothetical case where the first line of the file is indented
  // is not worth the complication.
  mut_Marker indentation = {
    .start = start_of_line - start,
    .len   = 0,
    .token_type = T_SPACE
  };
  Byte_mut_p end_of_indentation = start_of_line + 1;
  while (end_of_indentation is_not end) {
    if (*end_of_indentation is_not ' ' &&
        *end_of_indentation is_not '\t') break;
    ++end_of_indentation;
  }
  indentation.start = start_of_line      - start;
  indentation.len   = end_of_indentation - start_of_line;
  return indentation;
}

/** Copy the characters between `start` and `end` into the given slice,
 *  as many as they fit. If the slice is too small, the content is truncated
 *  with a Unicode ellipsis symbol “…” at the end.
 *
 *  To extract the text for `Marker_p m` from `Byte_p src`:
 *  ```
 *  extract_string(src->items + m->start,
 *                 src->items + m->start + m->len,
 *                 &slice);
 *  ```
 *
 *  @param[in] start marker pointer.
 *  @param[in] end marker pointer.
 *  @param[in] src byte buffer with the source code.
 *  @param[out] string Byte buffer to receive the bytes copied from the segment.
 */
void extract_src(Marker_p start, Marker_p end,
                 Buffer_p src, mut_Buffer_p string)
{
  if (end > start) {
    SourceCode start_byte = src->items + start->start;
    SourceCode   end_byte = src->items + (end-1)->start + (end-1)->len;
    Byte_array_slice insert = {
      .start_p = start_byte,
      .end_p   =   end_byte
    };
    splice_Byte_array(string, string->len, 0, &insert);
  }
}

void push_str(mut_Buffer_p _, const char * const str)
{
  Byte_array_slice insert = {
    .start_p = (Byte_p) str,
    .end_p   = (Byte_p) str + strlen(str)
  };
  splice_Byte_array(_, _->len, 0, &insert);
}

const char * as_c_string(mut_Buffer_p _)
{
  if (_->len == _->capacity) {
    Byte terminator = '\0';
    push_Byte_array(_, &terminator);
    --_->len;
  } else {
    *((mut_Byte_p) _->items + _->len) = 0;
  }
  return (const char * const) _->items;
}

/** Indent by the given number of double spaces, for the next `fprintf()`. */
void indent_log(size_t indent)
{
  if (indent > 40) indent = 40;
  while (indent-- is_not 0) fprintf(stderr, "  ");
}
#define log_indent(indent, ...) { indent_log(indent); log(__VA_ARGS__); }

/** Build a new marker for the given string,
 *  pointing to its first appearance in `src`.
 *  If not found, append the text to `src`
 *  and return a marker poiting there.
 */
Marker new_marker(mut_Buffer_p src,
                  const char * const text, TokenType token_type)
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
      while (*p2 && p1 != end && *p1 == *p2) { ++p1; ++p2; }
      if (*p2 is 0) {
        match = cursor;
        break;
      }
    }
  }
  mut_Marker marker = { .start = 0, .len = text_len, .token_type = token_type };
  if (match) {
    marker.start = match - Byte_array_start(src);
  } else {
    marker.start = src->len;
    Byte_mut_p p2 = (Byte_p) text;
    while (*p2) push_Byte_array(src, p2++);
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
void print_markers(Marker_array_p markers, Buffer_p src,
                   size_t start, size_t end,
                   Options options)
{
  size_t indent = 0;
  mut_Buffer token_text;
  init_Buffer(&token_text, 80);

  const char * const spacing = "                                ";
  const size_t spacing_len = strlen(spacing);

  Marker_p m_start =
      get_Marker_array(markers, start);
  Marker_p m_end   = end?
      get_Marker_array(markers, end):
      Marker_array_end(markers);
  const char * token = NULL;
  for (Marker_mut_p m = m_start; m is_not m_end; ++m) {
    extract_src(m, m+1, src, &token_text);
    token = as_c_string(&token_text);

    const char * const spc = m->len <= spacing_len?
        spacing + m->len:
        spacing + spacing_len;

    switch (m->token_type) {
      case T_BLOCK_START: case T_TUPLE_START: case T_INDEX_START:
        log_indent(indent++,
                   "%s %s← %s", token, spc, TOKEN_NAME[m->token_type]);
        break;
      case T_BLOCK_END: case T_TUPLE_END: case T_INDEX_END:
        log_indent(--indent,
                   "%s %s← %s", token, spc, TOKEN_NAME[m->token_type]);
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
        log_indent(indent, "%s %s← %s", token, spc, TOKEN_NAME[m->token_type]);
    }
  }

  destruct_Buffer(&token_text);
}

/* Format the markers back into source code form.
 * @param[in] markers tokens for the current form of the program.
 * @param[in] src original source code.
 * @param[in] options formatting options.
 * @param[in] out FILE pointer where the source code will be written.
 */
void unparse(Marker_array_p markers, Buffer_p src, Options options, FILE* out)
{
  Marker_p m_end = Marker_array_end(markers);
  bool eol_pending = false;
  for (Marker_mut_p m = (Marker_mut_p) markers->items; m is_not m_end; ++m) {
    if (options.discard_comments && m->token_type is T_COMMENT) {
      if (options.discard_space && not eol_pending &&
          m+1 is_not m_end && (m+1)->token_type is T_SPACE) ++m;
      continue;
    }
    Byte_p text = src->items + m->start;
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
            len = m->len - (eol - text);
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
    fwrite(text, sizeof *src->items, m->len, out);
  }
}

/**
   Match a keyword or identifier.
   @param[in] start of source code segment to search in.
   @param[in] end of source code segment.

   Keywords:

   - `T_CONTROL_FLOW`:
   `break case continue default do else for goto if return switch while`

   - `T_TYPE`:
   `char double enum float int long short struct union void`
   (c99: `bool complex imaginary`)

   - `T_TYPE_QUALIFIER`:
   `auto const extern inline register signed static unsigned volatile`
   (c99: `restrict`)

   - `T_OP_2`:
   `sizeof _Alignof`
 */
TokenType keyword_or_identifier(SourceCode start, SourceCode end)
{
  switch (end - start) {
    case 2:
      if (mem_eq(start, "do", 2) || mem_eq(start, "if", 2)) {
        return T_CONTROL_FLOW;
      }
      break;
    case 3:
      if (mem_eq(start, "for", 3)) {
        return T_CONTROL_FLOW;
      }
      if (mem_eq(start, "int", 3)) {
        return T_TYPE;
      }
      break;
    case 4:
      if (mem_eq(start, "case", 4) || mem_eq(start, "else", 4) ||
          mem_eq(start, "goto", 4)) {
        return T_CONTROL_FLOW;
      }
      if (mem_eq(start, "char", 4) || mem_eq(start, "enum", 4) ||
          mem_eq(start, "long", 4) || mem_eq(start, "void", 4) ||
          mem_eq(start, "bool", 4)) {
        return T_TYPE;
      }
      if (mem_eq(start, "auto", 4)) {
        return T_TYPE_QUALIFIER;
      }
      break;
    case 5:
      if (mem_eq(start, "break", 5) || mem_eq(start, "while", 5)) {
        return T_CONTROL_FLOW;
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
      if (mem_eq(start, "return", 6) || mem_eq(start, "switch", 6)) {
        return T_CONTROL_FLOW;
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

/* Parse the given source code into the `markers` array,
   appending the new markers to whatever was already there.
   Remember to empty the `markers` array before calling this function
   if you are re-parsing from scratch. */
SourceCode parse(Buffer_p src, mut_Marker_array_p markers)
{
  assert(PADDING_Buffer >= 8); // Must be greater than the longest keyword.
  mut_SourceCode cursor = src->items;
  SourceCode end = src->items + src->len;
  mut_SourceCode prev_cursor = 0;
  bool previous_token_is_value = false;
  bool include_mode = false;
  bool define_mode  = false;
  while (cursor is_not end) {
    if (cursor is prev_cursor) {
      log("ERROR: endless loop after %ld.\n", cursor - src->items);
      break;
    }
    prev_cursor = cursor;

    TokenType token_type = T_NONE;
    mut_SourceCode token_end = NULL;
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
      if ((include_mode||define_mode) && memchr(cursor, '\n', token_end - cursor)) {
        include_mode = false;
        if (define_mode) {
          define_mode = false;
          // TODO: mark somehow that the definition ends here.
        }
      }
    } else if ((token_end = preprocessor(cursor, end))) {
      TOKEN1(T_PREPROCESSOR);
      if (token_end is cursor) { log("error T_PREPROCESSOR"); break; }
      if (mem_eq(cursor, "#include", token_end - cursor)) {
        include_mode = true;
      } else if (mem_eq(cursor, "#define", token_end - cursor)) {
        define_mode = true;
      }
    } else if (include_mode && (token_end = system_include(cursor, end))) {
      TOKEN1(T_STRING);
      if (token_end is cursor) { log("error T_STRING"); break; }
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
                cursor - src->items, token_end - src->items, token_type);
    push_Marker_array(markers, &marker);
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

Marker_p
find_line_start(Marker_p cursor, Marker_p start, mut_Error_p err)
{
  Marker_mut_p start_of_line = cursor;
  size_t nesting = 0;
  while (start_of_line >= start) {
    switch (start_of_line->token_type) {
      case T_SEMICOLON:
        if (nesting is 0) {
          ++start_of_line;
          goto found;
        }
        break;
      case T_BLOCK_START: case T_TUPLE_START: case T_INDEX_START:
        if (nesting is 0) {
          ++start_of_line;
          goto found;
        } else {
          --nesting;
        }
        break;
      case T_BLOCK_END: case T_TUPLE_END: case T_INDEX_END:
        ++nesting;
        break;
      default: break;
    }
    --start_of_line;
  }
found:

  if (nesting is_not 0 or start_of_line < start) {
    err->message = "Excess group closings.";
    err->position = cursor->start;
  }

  return start_of_line;
}

Marker_p
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
    err->position = cursor->start;
  }

  return end_of_line;
}


/** Resolve the types of expressions. W.I.P. */
void resolve_types(mut_Marker_array_p markers, Buffer_p src)
{
  //uint8_t slice_data[80];
  //Buffer slice = { 80, 0, (uint8_t *)&slice_data };
  mut_Marker_p m_start = (mut_Marker_p) Marker_array_start(markers);
  mut_Marker_p m_end   = (mut_Marker_p) Marker_array_end(markers);
  //mut_SourceCode token = NULL;
  mut_Marker_mut_p previous = NULL;
  for (mut_Marker_mut_p m = m_start; m is_not m_end; ++m) {
    if (T_SPACE is m->token_type || T_COMMENT is m->token_type) continue;
    if (previous) {
      if ((T_TUPLE_START is m->token_type || T_OP_14 is m->token_type) &&
          T_IDENTIFIER  is previous->token_type) {
        mut_Marker_mut_p p = previous;
        while (p is_not m_start) {
          --p;
          switch (p->token_type) {
            case T_TYPE_QUALIFIER:
            case T_TYPE_STRUCT:
            case T_TYPE:
            case T_SPACE:
            case T_COMMENT:
              continue;
            case T_IDENTIFIER:
              p->token_type = T_TYPE;
              break;
            case T_OP_3:
              p->token_type = T_OP_2;
              break;
            default:
              p = m_start;
          }
        }
      }
    }
    previous = m;
  }
}


typedef FILE* mut_File_p;
typedef char*const FilePath;
/** Read a file into the given buffer. Errors are printed to `stderr`. */
void read_file(mut_Buffer_p _, FilePath path)
{
  mut_File_p input = fopen(path, "r");
  if (!input) {
    fprintf(stderr, "File not found: %s", path);
    init_Buffer(_, 0);
    return;
  }
  fseek(input, 0, SEEK_END);
  size_t size = ftell(input);
  init_Buffer(_, size);
  rewind(input);
  _->len = fread((mut_Byte_p) _->items, sizeof *_->items, size, input);
  if (feof(input)) {
    fprintf(stderr, "Unexpected EOF at %ld reading “%s”.\n", _->len, path);
  } else if (ferror(input)) {
    fprintf(stderr, "Error reading “%s”.\n", path);
  } else {
    fprintf(stderr, "Read %ld bytes from “%s”.\n", _->len, path);
    memset((mut_Byte_p) _->items + _->len, 0,
           (_->capacity - _->len) * sizeof *_->items);
  }
  fclose(input); input = NULL;
}

/***************** macros *****************/
#include "macros/fn.h"
#include "macros/let.h"
#include "macros/function_pointer.h"
#include "macros/backstitch.h"
#include "macros/count_markers.h"

const char* const usage_es =
    "Uso: cedro [opciones] fichero.c [fichero2.c … ]\n"
    "  --discard-spaces   Descarta los espacios en blanco. (implícito)\n"
    "  --discard-comments Descarta los comentarios. (implícito)\n"
    "  --print-markers    Imprime los marcadores.\n"
    "  --not-discard-spaces   No descarta los espacios.\n"
    "  --not-discard-comments No descarta los comentarios.\n"
    "  --not-print-markers    No imprime los marcadores. (implícito)\n"
    "\n"
    "  --enable-core-dump     Activa volcado de memoria al estrellarse.\n"
    "                         (implícito)\n"
    "  --not-enable-core-dump Desactiva volcado de memoria al estrellarse.\n"
    "  --version          Muestra la versión: " CEDRO_VERSION "\n"
    ;
const char* const usage_en =
    "Usage: cedro [options] file.c [file2.c … ]\n"
    "  --discard-spaces   Discards all whitespace. (default)\n"
    "  --discard-comments Discards the comments. (default)\n"
    "  --print-markers    Prints the markers.\n"
    "  --not-discard-spaces   Does not discard whitespace.\n"
    "  --not-discard-comments Does not discard comments.\n"
    "  --not-print-markers    Does not print the markers. (default)\n"
    "\n"
    "  --enable-core-dump     Enable core dump on crash. (default)\n"
    "  --not-enable-core-dump Disable core dump on crash.\n"
    "  --version          Show version: " CEDRO_VERSION "\n"
    ;

int main(int argc, char** argv)
{
  Options options = { // Remember to keep the usage strings updated.
    .discard_comments = true,
    .discard_space    = true,
    .apply_macros     = true,
    .print_markers    = false,
  };

  bool enable_core_dump = true;

  for (int i = 1; i < argc; ++i) {
    char* arg = argv[i];
    if (arg[0] is '-') {
      bool flag_value = true;
      if (0 is strncmp("--not-", arg, 6)) flag_value = false;
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
      } else if (str_eq("--version", arg)) {
        fprintf(stderr, CEDRO_VERSION "\n");
      } else {
        fprintf(stderr,
                0 is strncmp(getenv("LANG"), "es", 2)?
                usage_es:
                usage_en);
        return str_eq("-h", arg) || str_eq("--help", arg)? 0: 1;
      }
    }
  }

  if (enable_core_dump) {
    struct rlimit core_limit = { RLIM_INFINITY, RLIM_INFINITY };
    assert(0 == setrlimit(RLIMIT_CORE, &core_limit));
  }

  for (int i = 1; i < argc; ++i) {
    char* arg = argv[i];
    if (arg[0] is '-') continue;

    mut_Buffer src;
    read_file(&src, arg);

    mut_Marker_array markers;
    init_Marker_array(&markers, 8192);

    SourceCode cursor = parse(&src, &markers);

    resolve_types(&markers, &src);

    if (options.apply_macros) {
      log("Running macro count_markers:");
      macro_count_markers(&markers, &src);
      log("Running macro fn:");
      macro_fn(&markers, &src);
      log("Running macro let:");
      macro_let(&markers, &src);
      log("Running macro backstitch:");
      macro_backstitch(&markers, &src);
    }

    if (options.print_markers) {
      print_markers(&markers, &src, 0, 0, options);
    } else {
      unparse(&markers, &src, options, stderr);
    }

    destruct_Marker_array(&markers);

    log("\nRead %ld lines.",
        line_number((Buffer_p)&src, cursor - src.items) - 1);

    destruct_Buffer(&src);
  }

  return 0;
}
