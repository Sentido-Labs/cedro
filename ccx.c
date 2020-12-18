/* Author: Alberto González Palomo https://sentido-labs.com
 * ©2020   Alberto González Palomo https://sentido-labs.com
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

typedef struct Buffer {
  size_t len;
  size_t padding;
  uint8_t* bytes;
} Buffer, * const Buffer_p, * mut_Buffer_p;
void Buffer_init(mut_Buffer_p buffer, size_t len, size_t padding)
{
  buffer->bytes = calloc(1, len + padding);
  buffer->len = len;
  buffer->padding = padding;
}
void Buffer_drop(mut_Buffer_p buffer)
{
  buffer->len = 0;
  buffer->padding = 0;
  free(buffer->bytes);
  buffer->bytes = NULL;
}

typedef uint8_t*const SourceCode;// 0-terminated.
typedef uint8_t* mut_SourceCode; // 0-terminated.

/* https://en.cppreference.com/w/c/language/operator_precedence */
typedef enum TokenType {
  T_NONE,
  T_IDENTIFIER, T_TYPE, T_TYPE_QUALIFIER, T_CONTROL_FLOW,
  T_NUMBER, T_STRING, T_CHARACTER,
  T_SPACE, T_COMMENT,
  T_PREPROCESSOR,
  T_FUNCTION_CALL,
  T_BLOCK_START, T_BLOCK_END, T_TUPLE_START, T_TUPLE_END,
  T_INDEX_START, T_INDEX_END,
  T_GROUP_START, T_GROUP_END,
  T_OP_1, T_OP_2, T_OP_3, T_OP_4, T_OP_5, T_OP_6, T_OP_7, T_OP_8,
  T_OP_9, T_OP_10, T_OP_11, T_OP_12, T_OP_13, T_OP_14,
  T_COMMA /* = T_OP_15 */, T_SEMICOLON,
  T_OTHER
} TokenType;
static const unsigned char * const TOKEN_NAME[] = {
  "None",
  "Identifier", "Type", "Type qualifier", "Control flow",
  "Number", "String", "Character",
  "Space", "Comment",
  "Preprocessor",
  "Function call",
  "Block start", "Block end", "Tuple start", "Tuple end",
  "Index start", "Index end",
  "Group start", "Group end",
  "Op 1", "Op 2", "Op 3", "Op 4", "Op 5", "Op 6", "Op 7", "Op 8",
  "Op 9", "Op 10", "Op 11", "Op 12", "Op 13", "Op 14",
  "Comma (op 15)", "Semicolon",
  "OTHER"
};

#define precedence(token_type) (token_type - T_OP_1)
#define is_operator(token_type) (token_type >= T_OP_1 && token_type <= T_COMMA)
#define is_fence(token_type) (token_type >= T_BLOCK_START && token_type <= T_GROUP_END)

typedef struct Marker {
  size_t start;
  size_t end;
  TokenType token_type;
} Marker, * const Marker_p, * mut_Marker_p;

void Marker_init(mut_Marker_p _, size_t start, size_t end, TokenType token_type)
{
  _->start      = start;
  _->end        = end;
  _->token_type = token_type;
}

typedef struct Marker_array {
  size_t len;
  size_t capacity;
  Marker* items;
} Marker_array, * const Marker_array_p, * mut_Marker_array_p;

/** Example: <code>Marker_slice s = { 0, 10, &my_array };</code> */
typedef struct Marker_slice {
  size_t start;
  size_t end; /** 0 means the end of the array. */
  Marker_array_p array_p;
} Marker_slice, * const Marker_slice_p, * mut_Marker_slice_p;

void Marker_array_init(mut_Marker_array_p _, size_t initial_capacity)
{
  _->len = 0;
  _->capacity = initial_capacity;
  _->items = malloc(initial_capacity * sizeof(Marker));
  /* Used malloc() here instead of calloc() because we need realloc() later
     anyway, so better keep the exact same behaviour. */
}
void Marker_array_drop(mut_Marker_array_p _)
{
  _->len = 0;
  _->capacity = 0;
  free(_->items);
  _->items = NULL;
}

mut_Marker_p Marker_array_push(mut_Marker_array_p _,
                               size_t start, size_t end, TokenType token_type)
{
  if (_->capacity < _->len + 1) {
    _->capacity *= 2;
    _->items = realloc(_->items, _->capacity * sizeof(Marker));
  }
  Marker_init(&(_->items[_->len++]), start, end, token_type);
}

mut_Marker_array_p Marker_array_splice(mut_Marker_array_p _, size_t position, size_t delete, Marker_slice_p insert)
{
  size_t insert_len = insert->end;
  if (!insert_len) {
    insert_len = insert->array_p->len;
    if (insert_len) --insert_len;
  }
  if (insert_len > insert->start) insert_len -= insert->start;

  if (_->len + insert_len - delete >= _->capacity) {
    _->capacity *= 2;
    _->items = realloc(_->items, _->capacity * sizeof(Marker));
  }

  size_t gap_end = position + insert_len - delete;
  memmove(_->items + gap_end, _->items + position + delete,
          (_->len - delete - position) * sizeof(Marker));
  _->len = _->len + insert_len - delete;
  memmove(_->items + position, insert->array_p->items + insert->start,
          insert_len * sizeof(Marker));
}
mut_Marker_array_p Marker_array_delete(mut_Marker_array_p _, size_t position, size_t delete)
{
  memmove(_->items + position, _->items + position + delete,
          (_->len - delete - position) * sizeof(Marker));
  _->len -= delete;
}
mut_Marker_array_p Marker_array_pop(mut_Marker_array_p _, mut_Marker_p item_p)
{
  if (_->len) {
    mut_Marker_p last_p = _->items + _->len - 1;
    memmove(last_p, item_p, sizeof(Marker));
    --_->len;
  }
}
Marker_p Marker_array_end(Marker_array_p _)
{
  return _->items + _->len;
}

void define_macro(const char * const name, void (*f)(mut_Marker_array_p markers, Buffer_p src)) {
  fprintf(stderr, "(defmacro %s)\n", name);
}

#define defmacro(name, ...) \
  void macro_##name(mut_Marker_array_p markers, Buffer_p src) __VA_ARGS__
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

/* Add 10 bytes after end of buffer to avoid bounds checking while scanning
   for tokens. */
#define SRC_EXTRA_PADDING 10

SourceCode identifier(SourceCode start, SourceCode end)
{
  if (end <= start) return NULL;
  mut_SourceCode cursor = start;
  switch (*cursor) {
    case 'a' ... 'z': case 'A' ... 'Z': case '_':
      ++cursor;
      while (cursor < end) {
        switch (*cursor) {
          case '0' ... '9': case 'a' ... 'z': case 'A' ... 'Z': case '_':
            ++cursor;
            break;
          default:
            return cursor;
            // Unreachable: break;
        }
      }
      return cursor;
      // Unreachable: break;
    default:
      return NULL;
  }
}

/* This matches invalid numbers like 3.4.6, 09, and 0uL348.34e+11.
   Rejecting that is left to the compiler. */
SourceCode number(SourceCode start, SourceCode end)
{
  if (end <= start) return NULL;
  mut_SourceCode cursor = start;
  switch (*cursor) {
    case '0' ... '9':
      ++cursor;
      while (cursor < end) {
        switch (*cursor) {
          case '0' ... '9': case '.':
            ++cursor;
            break;
          case 'u': case 'U': case 'l': case 'L':
            ++cursor;
            if (cursor < end) {
              switch (*cursor) {
                case 'u': case 'U': case 'l': case 'L': ++cursor;
              }
            }
            break;
          case 'E': case 'e':
            ++cursor;
            if (cursor < end) {
              switch (*cursor) { case '+': case '-': ++cursor; }
              while (cursor < end) {
                switch (*cursor) { case '0' ... '9': ++cursor; }
              }
            }
            break;
          default:
            return cursor;
            // Unreachable: break;
        }
      }
      return cursor;
      // Unreachable: break;
    default:
      return NULL;
  }
}

SourceCode string(SourceCode start, SourceCode end)
{
  if (*start is_not '"' or end <= start) return NULL;
  mut_SourceCode cursor = start + 1;
  while (cursor < end and *cursor is_not '"') {
    if (*cursor is '\\') ++cursor;
    ++cursor;
  }
  return (cursor is end)? NULL: cursor + 1;// End is past the closing quotes.
}

SourceCode system_include(SourceCode start, SourceCode end)
{
  if (*start is_not '<' or end <= start) return NULL;
  mut_SourceCode cursor = start + 1;
  while (cursor < end and *cursor is_not '>') ++cursor;
  return (cursor is end)? NULL: cursor + 1;// End is past the closing quotes.
}

SourceCode character(SourceCode start, SourceCode end)
{
  if (*start is_not '\'' or end <= start) return NULL;
  mut_SourceCode cursor = start + 1;
  while (cursor < end and *cursor is_not '\'') {
    if (*cursor is '\\') ++cursor;
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
  switch (*cursor) {
    case 'a' ... 'z': case 'A' ... 'Z': case '_':
      ++cursor;
      while (cursor < end) {
        switch (*cursor) {
          case '0' ... '9': case 'a' ... 'z': case 'A' ... 'Z': case '_':
            ++cursor;
            break;
          default:
            return cursor;
            // Unreachable: break;
        }
      }
      return cursor;
      // Unreachable: break;
    case '#':
      // Token concatenation.
      return cursor + 1;
      // Unreachable: break;
    default:
      return NULL;
  }
}



size_t line_number(Buffer_p src, SourceCode cursor)
{
  size_t line_number = 1;
  if (cursor > src->bytes && cursor - src->bytes <= src->len) {
    mut_SourceCode pointer = cursor;
    while (pointer is_not src->bytes) if (*(--pointer) is '\n') ++line_number;
  }
  return line_number;
}

SourceCode extract_string(SourceCode start, SourceCode end, mut_Buffer_p slice)
{
  size_t len = end - start;
  if (len >= slice->len) len = slice->len - 1;
  memcpy(slice->bytes, start, len);
  slice->bytes[len] = 0;
  return slice->bytes;
}

void indent_log(size_t indent)
{
  if (indent > 40) indent = 40;
  while (indent is_not 0) {
    fprintf(stderr, "  ");
    --indent;
  }
}
#define log_indent(indent, ...) { indent_log(indent); log(__VA_ARGS__); }

/** Print a human-legible dump of the markers array to stderr. */
void dump_markers(Marker_array_p markers, Buffer_p src,
                  bool ignore_comment, bool ignore_space)
{
  size_t indent = 0;
  uint8_t slice_data[80];
  Buffer slice = { 80, 0, (uint8_t *)&slice_data };
  const char * const spacing = "                                ";
  const size_t spacing_len = 32;

  Marker_p m_end = Marker_array_end(markers);
  mut_SourceCode token = NULL;
  for (mut_Marker_p m = markers->items; m is_not m_end; ++m) {
    token = extract_string(src->bytes + m->start,
                           src->bytes + m->end,
                           &slice);

    const char * const spc = m->end - m->start <= spacing_len?
        spacing + m->end - m->start:
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
        if (not ignore_space) {
          log_indent(indent, "%s %s← Space", token, spc);
        }
        break;
      case T_COMMENT:
        if (not ignore_comment) {
          log_indent(indent, "%s %s← Comment", token, spc);
        }
        break;
      default:
        log_indent(indent, "%s %s← %s", token, spc, TOKEN_NAME[m->token_type]);
    }
  }
}

/*
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
  mut_SourceCode cursor = src->bytes;
  SourceCode end = src->bytes + src->len;
  mut_SourceCode prev_cursor = 0;
  bool previous_token_is_value = false;
  bool include_mode = false;
  while (cursor is_not end) {
    if (cursor == prev_cursor) {
      log("ERROR: endless loop after %d.\n", cursor - src->bytes);
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
      if (include_mode && memchr(cursor, '\n', token_end - cursor)) {
        include_mode = false;
      }
    } else if ((token_end = preprocessor(cursor, end))) {
      TOKEN1(T_PREPROCESSOR);
      if (token_end == cursor) { log("error T_PREPROCESSOR"); break; }
      if (0 == memcmp(cursor, "#include", token_end - cursor)) {
        include_mode = true;
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
        case '.': TOKEN1(T_OP_1);        break;
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
    Marker_array_push(markers,
                      cursor - src->bytes, token_end - src->bytes, token_type);
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
    if (T_TUPLE_START == m->token_type && previous &&
        T_IDENTIFIER  == previous->token_type) {
      previous->token_type = T_FUNCTION_CALL;
    }
    previous = m;
  }
}


typedef FILE* mut_File_p;
typedef char*const FilePath;
Buffer read_file(FilePath path)
{
  Buffer buffer;
  mut_File_p input = fopen(path, "r");
  fseek(input, 0, SEEK_END);
  Buffer_init(&buffer, ftell(input), SRC_EXTRA_PADDING);
  rewind(input);
  fread(buffer.bytes, 1, buffer.len, input);
  fclose(input); input = NULL;

  return buffer;
}

//char* pattern[] = { "Identifier: DEF", "Start parenthetical expression.", "Identifier: \1", "Comma.", "Start block.", "Identifier: \2" };

int main(int argc, char** argv)
{
  log("Hello, start parsing.");
  define_macros();

  Buffer src = read_file("test.c");

  bool ignore_space = true;
  bool ignore_comment = false;

  Marker_array markers;
  Marker_array_init(&markers, 8192);

  SourceCode cursor = parse((Buffer_p)&src, (mut_Marker_array_p)&markers);

  resolve_types((mut_Marker_array_p)&markers, (Buffer_p)&src);

  macro_count_markers((mut_Marker_array_p)&markers, (Buffer_p)&src);
  macro_fn((mut_Marker_array_p)&markers, (Buffer_p)&src);
  macro_let((mut_Marker_array_p)&markers, (Buffer_p)&src);

  dump_markers((Marker_array_p)&markers, (Buffer_p)&src,
               ignore_comment, ignore_space);

  Marker_array_drop(&markers);

  log("Read %d lines.", line_number((Buffer_p)&src, cursor) - 1);

  Buffer_drop(&src);

  return 0;
}
