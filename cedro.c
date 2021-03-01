/**
 * Author: Alberto González Palomo https://sentido-labs.com
 * ©2021 Alberto González Palomo https://sentido-labs.com
 * Created: 2020-11-25 22:41
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <iso646.h>
#define is     ==
#define is_not not_eq

#include <string.h> // For memcpy(), memmove().

typedef struct Options {
  bool ignore_space;
  bool ignore_comment;
} Options, *Options_p;

typedef uint8_t*const SourceCode;/// 0-terminated.
typedef uint8_t* mut_SourceCode; /// 0-terminated.

#define B(string) ((unsigned char*)string)

/** https://en.cppreference.com/w/c/language/operator_precedence */
typedef enum TokenType {
  T_NONE,
  T_IDENTIFIER, T_TYPE, T_TYPE_QUALIFIER, T_CONTROL_FLOW,
  T_NUMBER, T_STRING, T_CHARACTER,
  T_SPACE, T_COMMENT,
  T_PREPROCESSOR,
  T_BLOCK_START, T_BLOCK_END, T_TUPLE_START, T_TUPLE_END,
  T_INDEX_START, T_INDEX_END,
  T_GROUP_START, T_GROUP_END,
  T_OP_1, T_OP_2, T_OP_3, T_OP_4, T_OP_5, T_OP_6, T_OP_7, T_OP_8,
  T_OP_9, T_OP_10, T_OP_11, T_OP_12, T_OP_13, T_OP_14,
  T_COMMA /* = T_OP_15 */, T_SEMICOLON,
  T_ELLIPSIS,
  T_OTHER
} TokenType;
static const unsigned char * const TOKEN_NAME[] = {
  B("None"),
  B("Identifier"), B("Type"), B("Type qualifier"), B("Control flow"),
  B("Number"), B("String"), B("Character"),
  B("Space"), B("Comment"),
  B("Preprocessor"),
  B("Block start"), B("Block end"), B("Tuple start"), B("Tuple end"),
  B("Index start"), B("Index end"),
  B("Group start"), B("Group end"),
  B("Op 1"), B("Op 2"), B("Op 3"), B("Op 4"), B("Op 5"), B("Op 6"),
  B("Op 7"), B("Op 8"), B("Op 9"), B("Op 10"), B("Op 11"), B("Op 12"),
  B("Op 13"), B("Op 14"),
  B("Comma (op 15)"), B("Semicolon"),
  B("Ellipsis"),
  B("OTHER")
};

#define precedence(token_type) (token_type - T_OP_1)
#define is_operator(token_type) (token_type >= T_OP_1 && token_type <= T_COMMA)
#define is_fence(token_type) (token_type >= T_BLOCK_START && token_type <= T_GROUP_END)

/** Defines mutable types mut_〈T〉 and mut_〈T〉_p (pointer),
    and constant types 〈T〉 and 〈T〉_p (pointer to constant). */
#define TYPEDEF(T, STRUCT)                                       \
  typedef struct T STRUCT mut_##T, * const T##_p, * mut_##T##_p; \
  typedef const struct T T

TYPEDEF(Marker, {
  size_t start;
  size_t len;
  TokenType token_type;
  });

void init_Marker(mut_Marker_p _, size_t start, size_t end, TokenType token_type)
{
  _->start      = start;
  _->len        = end - start;
  _->token_type = token_type;
}
void drop_Marker(mut_Marker_p _) {}

#define DEFINE_ARRAY_OF(T, PADDING)                                     \
/** There must be a drop_##T(T##_p _) function. */                      \
typedef struct T##_array {                                              \
  size_t len;                                                           \
  size_t capacity;                                                      \
  mut_##T* items;                                                       \
  mut_##T##_p last_p;                                                   \
} mut_##T##_array, * const T##_array_p, * mut_##T##_array_p;            \
typedef const struct T##_array T##_array;                               \
                                                                        \
/** Example: <code>T##_slice s = { 0, 10, &my_array };</code> */        \
typedef struct T##_array_slice {                                        \
  size_t start;                                                         \
  size_t end; /** 0 means the end of the array. */                      \
  T##_array_p array_p;                                                  \
} T##_array_slice, * const T##_array_slice_p, * mut_##T##_array_slice_p; \
                                                                        \
/** Initialize the array at the given pointer.                          \
    For local variables, use it like this:                              \
    \code{.c}                                                           \
    T##_array things;                                                   \
    T##_array_init(&things, 100); // We expect around 100 items.        \
    {...}                                                               \
    T##_array_drop(&things);                                            \
    \endcode                                                            \
 */                                                                     \
mut_##T##_array_p init_##T##_array(mut_##T##_array_p _, size_t initial_capacity) \
{                                                                       \
  _->len = 0;                                                           \
  _->capacity = initial_capacity + PADDING;                             \
  _->items = malloc(_->capacity * sizeof(T));                           \
  /* Used malloc() here instead of calloc() because we need realloc() later \
     anyway, so better keep the exact same behaviour. */                \
  _->last_p = NULL;                                                     \
  return _;                                                             \
}                                                                       \
/** Release any resources allocated for this struct.                    \
    Returns `NULL` to enable convenient clearing of the pointer:        \
    `p = drop_##T##_array(p); // p is now NULL.` */                     \
mut_##T##_array_p drop_##T##_array(mut_##T##_array_p _)                 \
{                                                                       \
  mut_##T##_p cursor = _->items;                                        \
  T##_p       end    = _->items + _->len;                               \
  while (cursor != end) drop_##T(cursor++);                             \
  _->len = 0;                                                           \
  _->capacity = 0;                                                      \
  _->last_p = NULL;                                                     \
  free(_->items); _->items = NULL;                                      \
  return _;                                                             \
}                                                                       \
                                                                        \
mut_##T##_array_p push_##T##_array(mut_##T##_array_p _, T##_p item)     \
{                                                                       \
  if (_->capacity < _->len + 1 + PADDING) {                             \
    _->capacity = 2*_->capacity + PADDING;                              \
    _->items = realloc(_->items, _->capacity * sizeof(T));              \
  }                                                                     \
  *(_->last_p = _->items + _->len++) = *item;                           \
  return _;                                                             \
}                                                                       \
mut_##T##_array_p splice_##T##_array(mut_##T##_array_p _, size_t position, size_t delete, T##_array_slice_p insert) \
{                                                                       \
  mut_##T##_p cursor = _->items + position;                             \
  T##_p       end    = _->items + position + delete;                    \
  while (cursor != end) drop_##T(cursor++);                             \
                                                                        \
  size_t insert_len = insert->end;                                      \
  if (!insert_len) {                                                    \
    insert_len = insert->array_p->len;                                  \
    if (insert_len) --insert_len;                                       \
  }                                                                     \
  if (insert_len > insert->start) insert_len -= insert->start;          \
                                                                        \
  if (_->len + insert_len - delete + PADDING >= _->capacity) {          \
    _->capacity = 2*_->capacity + PADDING;                              \
    _->items = realloc(_->items, _->capacity * sizeof(T));              \
  }                                                                     \
                                                                        \
  size_t gap_end = position + insert_len - delete;                      \
  memmove(_->items + gap_end, _->items + position + delete,             \
          (_->len - delete - position) * sizeof(T));                    \
  _->len = _->len + insert_len - delete;                                \
  memmove(_->items + position, insert->array_p->items + insert->start,  \
          insert_len * sizeof(T));                                      \
  return _;                                                             \
}                                                                       \
mut_##T##_array_p delete_##T##_array(mut_##T##_array_p _, size_t position, size_t delete) \
{                                                                       \
  mut_##T##_p cursor = _->items + position;                             \
  T##_p       end    = _->items + position + delete;                    \
  while (cursor != end) drop_##T(cursor++);                             \
                                                                        \
  memmove(_->items + position, _->items + position + delete,            \
          (_->len - delete - position) * sizeof(T));                    \
  _->len -= delete;                                                     \
  return _;                                                             \
}                                                                       \
mut_##T##_array_p pop_##T##_array(mut_##T##_array_p _, mut_##T##_p item_p) \
{                                                                       \
  if (not _->len) return NULL;                                          \
  mut_##T##_p last_p = _->items + _->len - 1;                           \
  if (item_p) memmove(last_p, item_p, sizeof(T));                       \
  else        drop_##T(last_p);                                         \
  --_->len;                                                             \
  return _;                                                             \
}                                                                       \
T##_p T##_array_start(T##_array_p _, size_t position)                   \
{                                                                       \
  if (position >= _->len) position = _->len;                            \
  return _->items + position;                                           \
}                                                                       \
T##_p T##_array_end(T##_array_p _)                                      \
{                                                                       \
  return _->items + _->len;                                             \
}                                                                       \
const size_t PADDING_##T##_ARRAY = PADDING//; commented out to avoid ;;.

DEFINE_ARRAY_OF(Marker, 0);

/** Add 10 bytes after end of buffer to avoid bounds checking while scanning
    for tokens. No literal token is that long. */
#define SRC_EXTRA_PADDING 10
typedef uint8_t mut_Byte, * const Byte_p, * mut_Byte_p;
typedef const uint8_t Byte;
#define drop_Byte(_) _
DEFINE_ARRAY_OF(Byte, SRC_EXTRA_PADDING);
typedef mut_Byte_array mut_Buffer, *mut_Buffer_p;
typedef     Byte_array     Buffer, *    Buffer_p;
#define init_Buffer init_Byte_array
#define drop_Buffer drop_Byte_array

void define_macro(const char * const name, void (*f)(mut_Marker_array_p markers, Buffer_p src)) {
  fprintf(stderr, "(defmacro %s)\n", name);
}

#define defmacro(name, body) \
  void macro_##name(mut_Marker_array_p markers, Buffer_p src) body const char const * macro_##name##_NAME = #name
#include "macros.h"
#undef  defmacro
#define defmacro(name, body) \
  define_macro(#name, &macro_##name)
void define_macros()
{
#include "macros.h"
}

#define log(...) { fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); }

/* Lexer definitions. */

SourceCode identifier(SourceCode start, SourceCode end)
{
  if (end <= start) return NULL;
  mut_SourceCode cursor = start;
  mut_Byte c = *cursor;
  if (c >= 'a' and c <= 'z' or c >= 'A' and c <= 'Z' or c == '_') {
    ++cursor;
    while (cursor < end) {
      c = *cursor;
      if (c >= '0' and c <= '9' or
          c >= 'a' and c <= 'z' or c >= 'A' and c <= 'Z' or c == '_') {
        ++cursor;
      } else break;
    }
    return cursor;
  } else {
    return NULL;
  }
}

/** This matches invalid numbers like 3.4.6, 09, and 3e23.48.34e+11.
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
      if (c >= '1' and c <= '9' or c == '.') {
        ++cursor;
      } else if (c == 'u' or c == 'U' or c == 'l' or c == 'L') {
        ++cursor;
        if (cursor < end) {
          c = *cursor;
          if (c == 'u' or c == 'U' or c == 'l' or c == 'L') ++cursor;
          else break;
        }
      } else if (c == 'e' or c == 'E') {
        ++cursor;
        if (cursor < end) {
          c = *cursor;
          if (c == '+' or c == '-') ++cursor;
          while (cursor < end) {
            c = *cursor;
            if (c >= '1' and c <= '9' or c == '.') ++cursor;
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

SourceCode string(SourceCode start, SourceCode end)
{
  if (*start is_not '"' or end <= start) return NULL;
  mut_SourceCode cursor = start + 1;
  while (cursor is_not end and *cursor is_not '"') {
    if (*cursor is '\\' && cursor + 1 is_not end) ++cursor;
    ++cursor;
  }
  return (cursor is end)? NULL: cursor + 1;// End is past the closing quotes.
}

SourceCode system_include(SourceCode start, SourceCode end)
{
  if (*start is_not '<' or end <= start) return NULL;
  mut_SourceCode cursor = start + 1;
  while (cursor is_not end and *cursor is_not '>') ++cursor;
  return (cursor is end)? NULL: cursor + 1;// End is past the closing quotes.
}

SourceCode character(SourceCode start, SourceCode end)
{
  if (*start is_not '\'' or end <= start) return NULL;
  mut_SourceCode cursor = start + 1;
  while (cursor is_not end and *cursor is_not '\'') {
    if (*cursor is '\\' && cursor + 1 is_not end) ++cursor;
    ++cursor;
  }
  return (cursor is end)? NULL: cursor + 1;// End is past the closing quote.
}

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
        // No need to check bounds thanks to SRC_EXTRA_PADDING.
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
  return (cursor is end)? NULL: cursor + 1;// End is past the closing quotes.
}

SourceCode comment_line(SourceCode start, SourceCode end)
{
  if (*start is_not '/' or end <= start + 1) return NULL;
  mut_SourceCode cursor = start + 1;
  if (*cursor is_not '/') return NULL;
}

SourceCode preprocessor(SourceCode start, SourceCode end)
{
  if (*start is_not '#' or end <= start) return NULL;
  mut_SourceCode cursor = start + 1;
  if (cursor == end) return cursor;
  mut_Byte c = *cursor;
  if (c >= 'a' and c <= 'z' or c >= 'A' and c <= 'Z' or c == '_') {
    ++cursor;
    while (cursor < end) {
      c = *cursor;
      if (c >= '0' and c <= '9' or
          c >= 'a' and c <= 'z' or c >= 'A' and c <= 'Z' or c == '_') {
        ++cursor;
      } else break;
    }
    return cursor;
  } else if (c == '#') {
    // Token concatenation.
    return cursor + 1;
  } else {
    return NULL;
  }
}



size_t line_number(Buffer_p src, SourceCode cursor)
{
  size_t line_number = 1;
  if (cursor > src->items && cursor - src->items <= src->len) {
    mut_SourceCode pointer = cursor;
    while (pointer is_not src->items) if (*(--pointer) is '\n') ++line_number;
  }
  return line_number;
}

SourceCode extract_string(SourceCode start, SourceCode end, mut_Buffer_p slice)
{
  size_t len = end - start;
  if (len >= slice->len - 4) {
    memcpy(slice->items, start, slice->len - 4);
    uint8_t* p = slice->items + slice->len - 4;
    *(p++) = 0xE2; *(p++) = 0x80; *(p++) = 0xA6;// UTF-8 ellipsis: “…”
    len = slice->len;
  } else {
    memcpy(slice->items, start, len);
  }
  slice->items[len] = 0;
  return slice->items;
}

void indent_log(size_t indent)
{
  if (indent > 40) indent = 40;
  while (indent-- is_not 0) fprintf(stderr, "  ");
}
#define log_indent(indent, ...) { indent_log(indent); log(__VA_ARGS__); }

/** Print a human-legible dump of the markers array to stderr. */
void dump_markers(Marker_array_p markers, Buffer_p src, Options options)
{
  size_t indent = 0;
  mut_Byte slice_data[80];
  mut_Buffer slice = { 80, 0, &slice_data[0] };
  const char * const spacing = "                                ";
  const size_t spacing_len = strlen(spacing);

  Marker_p m_end = Marker_array_end(markers);
  mut_SourceCode token = NULL;
  for (mut_Marker_p m = markers->items; m is_not m_end; ++m) {
    token = extract_string(src->items + m->start,
                           src->items + m->start + m->len,
                           &slice);

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
        /* Invisible grouping tokens. */
        break;
      case T_SPACE:
        if (not options.ignore_space) {
          log_indent(indent, "%s %s← Space", token, spc);
        }
        break;
      case T_COMMENT:
        if (not options.ignore_comment) {
          log_indent(indent, "%s %s← Comment", token, spc);
        }
        break;
      default:
        log_indent(indent, "%s %s← %s", token, spc, TOKEN_NAME[m->token_type]);
    }
  }
}

void unparse(Marker_array_p markers, Buffer_p src, Options options, FILE* out)
{
  Marker_p m_end = Marker_array_end(markers);
  for (mut_Marker_p m = markers->items; m is_not m_end; ++m) {
    if (options.ignore_comment && m->token_type == T_COMMENT) continue;
    if (options.ignore_space && m->token_type == T_SPACE) {
      fwrite(" ", 1, 1, out);
      continue;
    }
    fwrite(src->items + m->start, sizeof(uint8_t), m->len, out);
  }
}

/**
  T_CONTROL_FLOW:
  break case continue default do else for goto if return switch while
  T_TYPE:
  char double enum float int long short struct union void (c99: bool complex imaginary)
  T_TYPE_QUALIFIER:
  auto const extern inline register restrict? signed static unsigned volatile
  T_OP_2:
  sizeof _Alignof
 */
TokenType keyword_or_identifier(SourceCode start, SourceCode end)
{
  switch (end - start) {
    case 2:
      if (0 == memcmp(start, "do", 2) || 0 == memcmp(start, "if", 2)) {
        return T_CONTROL_FLOW;
      }
      break;
    case 3:
      if (0 == memcmp(start, "for", 3)) {
        return T_CONTROL_FLOW;
      }
      if (0 == memcmp(start, "int", 3)) {
        return T_TYPE;
      }
      break;
    case 4:
      if (0 == memcmp(start, "case", 4) || 0 == memcmp(start, "else", 4) ||
          0 == memcmp(start, "goto", 4)) {
        return T_CONTROL_FLOW;
      }
      if (0 == memcmp(start, "char", 4) || 0 == memcmp(start, "enum", 4) ||
          0 == memcmp(start, "long", 4) || 0 == memcmp(start, "void", 4) ||
          0 == memcmp(start, "bool", 4)) {
        return T_TYPE;
      }
      if (0 == memcmp(start, "auto", 4)) {
        return T_TYPE_QUALIFIER;
      }
      break;
    case 5:
      if (0 == memcmp(start, "break", 5) || 0 == memcmp(start, "while", 5)) {
        return T_CONTROL_FLOW;
      }
      if (0 == memcmp(start, "float", 5) || 0 == memcmp(start, "short", 5) ||
          0 == memcmp(start, "union", 5)) {
        return T_TYPE;
      }
      if (0 == memcmp(start, "const", 5)) {
        return T_TYPE_QUALIFIER;
      }
      break;
    case 6:
      if (0 == memcmp(start, "return", 6) || 0 == memcmp(start, "switch", 6)) {
        return T_CONTROL_FLOW;
      }
      if (0 == memcmp(start, "double", 6) || 0 == memcmp(start, "struct", 6)) {
        return T_TYPE;
      }
      if (0 == memcmp(start, "extern", 6) || 0 == memcmp(start, "inline", 6) ||
          0 == memcmp(start, "signed", 6) || 0 == memcmp(start, "static", 6)){
        return T_TYPE_QUALIFIER;
      }
      if (0 == memcmp(start, "sizeof", 6)) {
        return T_OP_2;
      }
      break;
    case 7:
      if (0 == memcmp(start, "default", 7)) {
        return T_CONTROL_FLOW;
      }
      if (0 == memcmp(start, "complex", 7)) {
        return T_TYPE;
      }
      break;
    case 8:
      if (0 == memcmp(start, "continue", 8)) {
        return T_CONTROL_FLOW;
      }
      if (0 == memcmp(start, "register", 8) || 0 == memcmp(start, "restrict", 8) ||
          0 == memcmp(start, "unsigned", 8) || 0 == memcmp(start, "volatile", 8)){
        return T_TYPE_QUALIFIER;
      }
      if (0 == memcmp(start, "_Alignof", 8)) {
        return T_OP_2;
      }
      break;
    case 9:
      if (0 == memcmp(start, "imaginary", 9)) {
        return T_TYPE;
      }
      break;
  }
  return T_IDENTIFIER;
}

#define TOKEN1(token) token_type = token
#define TOKEN2(token) ++token_end;    TOKEN1(token)
#define TOKEN3(token) token_end += 2; TOKEN1(token)

SourceCode parse(Buffer_p src, mut_Marker_array_p markers)
{
  mut_SourceCode cursor = src->items;
  SourceCode end = src->items + src->len;
  mut_SourceCode prev_cursor = 0;
  bool previous_token_is_value = false;
  bool include_mode = false;
  bool define_mode  = false;
  while (cursor is_not end) {
    if (cursor == prev_cursor) {
      log("ERROR: endless loop after %d.\n", cursor - src->items);
      break;
    }
    prev_cursor = cursor;

    TokenType token_type = T_NONE;
    mut_SourceCode token_end = NULL;
    if        ((token_end = identifier(cursor, end))) {
      TOKEN1(keyword_or_identifier(cursor, token_end));
      if (token_end == cursor) log("error T_IDENTIFIER");
    } else if ((token_end = string(cursor, end))) {
      TOKEN1(T_STRING);
      if (token_end == cursor) { log("error T_STRING"); break; }
    } else if ((token_end = number(cursor, end))) {
      TOKEN1(T_NUMBER);
      if (token_end == cursor) { log("error T_NUMBER"); break; }
    } else if ((token_end = character(cursor, end))) {
      TOKEN1(T_CHARACTER);
      if (token_end == cursor) { log("error T_CHARACTER"); break; }
    } else if ((token_end = comment(cursor, end))) {
      TOKEN1(T_COMMENT);
      if (token_end == cursor) { log("error T_COMMENT"); break; }
    } else if ((token_end = space(cursor, end))) {
      TOKEN1(T_SPACE);
      if (token_end == cursor) { log("error T_SPACE"); break; }
      if ((include_mode||define_mode) && memchr(cursor, '\n', token_end - cursor)) {
        include_mode = false;
        if (define_mode) {
          define_mode = false;
          // TODO: mark somehow that the definition ends here.
        }
      }
    } else if ((token_end = preprocessor(cursor, end))) {
      TOKEN1(T_PREPROCESSOR);
      if (token_end == cursor) { log("error T_PREPROCESSOR"); break; }
      if (0 == memcmp(cursor, "#include", token_end - cursor)) {
        include_mode = true;
      } else if (0 == memcmp(cursor, "#define", token_end - cursor)) {
        define_mode = true;
      }
    } else if (include_mode && (token_end = system_include(cursor, end))) {
      TOKEN1(T_STRING);
      if (token_end == cursor) { log("error T_STRING"); break; }
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
          if (c2 == '.' && c3 == '.') { TOKEN3(T_ELLIPSIS); }
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

void resolve_types(Marker_array_p markers, Buffer_p src)
{
  uint8_t slice_data[80];
  Buffer slice = { 80, 0, (uint8_t *)&slice_data };
  Marker_p markers_end = Marker_array_end(markers);
  mut_SourceCode token = NULL;
  mut_Marker_p previous = NULL;
  for (mut_Marker_p m = markers->items; m is_not markers_end; ++m) {
    if (T_SPACE == m->token_type || T_COMMENT == m->token_type) continue;
    if (previous) {
      if ((T_TUPLE_START == m->token_type || T_OP_14 == m->token_type) &&
          T_IDENTIFIER  == previous->token_type) {
        mut_Marker_p p = previous;
        while (p is_not markers->items) {
          --p;
          switch (p->token_type) {
            case T_TYPE_QUALIFIER:
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
              p = markers->items;
          }
        }
      }
    }
    previous = m;
  }
}


typedef FILE* mut_File_p;
typedef char*const FilePath;
void read_file(mut_Buffer_p _, FilePath path)
{
  mut_File_p input = fopen(path, "r");
  fseek(input, 0, SEEK_END);
  size_t size = ftell(input);
  init_Buffer(_, size);
  rewind(input);
  _->len = fread(_->items, sizeof(Byte), size, input);
  if (feof(input)) {
    fprintf(stderr, "Unexpected EOF at %d reading “%s”.\n", _->len, path);
  } else if (ferror(input)) {
    fprintf(stderr, "Error reading “%s”.\n", path);
  } else {
    fprintf(stderr, "Read %d bytes from “%s”.\n", _->len, path);
    memset(_->items + _->len, 0, (_->capacity - _->len) * sizeof(Byte));
  }
  fclose(input); input = NULL;
}

int main(int argc, char** argv)
{
  define_macros();

  for (int i = 1; i < argc; ++i) {
    char* arg = argv[i];
    if (arg[0] == '-') {
      char* lang = getenv("LANG");
      if (0 == strncmp(lang, "es", 2)) {
        fprintf(stderr, "Uso: cedro fichero.c [fichero2.c … ]\n");
      } else {
        fprintf(stderr, "Usage: cedro file.c [file2.c … ]\n");
      }
      return 1;
    }

    mut_Buffer src;
    read_file(&src, arg);

    Options options = { .ignore_comment = false, .ignore_space = true };

    mut_Marker_array markers;
    init_Marker_array(&markers, 8192);

    SourceCode cursor = parse((Buffer_p)&src, (mut_Marker_array_p)&markers);

    resolve_types((mut_Marker_array_p)&markers, (Buffer_p)&src);

    fprintf(stderr, "Running macro %s:\n", macro_count_markers_NAME);
    macro_count_markers((mut_Marker_array_p)&markers, (Buffer_p)&src);
    fprintf(stderr, "Running macro %s:\n", macro_fn_NAME);
    macro_fn((mut_Marker_array_p)&markers, (Buffer_p)&src);
    fprintf(stderr, "Running macro %s:\n", macro_let_NAME);
    macro_let((mut_Marker_array_p)&markers, (Buffer_p)&src);

    dump_markers((Marker_array_p)&markers, (Buffer_p)&src, options);

    options.ignore_space = false;
    unparse((Marker_array_p)&markers, (Buffer_p)&src, options, stderr);

    drop_Marker_array(&markers);

    log("Read %d lines.", line_number((Buffer_p)&src, cursor) - 1);

    drop_Buffer(&src);
  }

  return 0;
}
