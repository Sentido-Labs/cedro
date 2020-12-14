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

typedef FILE* mut_File_p;
typedef char*const FilePath;
typedef uint8_t*const SourceCode;// 0-terminated.
typedef uint8_t* mut_SourceCode; // 0-terminated.

/* Add 0 bytes after end of buffer to avoid bounds checking while scanning
   for tokens. */
#define SRC_EXTRA_PADDING 10

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

char* pattern[] = { "Identifier: DEF", "Start parenthetical expression.", "Identifier: \1", "Comma.", "Start block.", "Identifier: \2" };

typedef enum TokenType {
  T_NONE,
  T_IDENTIFIER, T_NUMBER, T_STRING, T_CHARACTER, T_SPACE, T_COMMENT,
  T_PREPROCESSOR,
  T_BLOCK_START, T_BLOCK_END, T_TUPLE_START, T_TUPLE_END,
  T_INDEX_START, T_INDEX_END,
  T_GROUP_START, T_GROUP_END,
  T_OP_1, T_OP_2, T_OP_3, T_OP_4, T_OP_5, T_OP_6, T_OP_7, T_OP_8,
  T_OP_9, T_OP_10, T_OP_11, T_OP_12, T_OP_13, T_OP_14, T_OP_15, T_OP_16,
  T_OP_3_2, T_OP_5_3, T_OP_6_3, T_OP_11_3,
  T_COMMA /* = T_OP_17 */, T_SEMICOLON,
  T_OTHER
} TokenType;

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
  Marker* markers;
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
  _->markers = malloc(initial_capacity * sizeof(Marker));
  /* Used malloc() here instead of calloc() because we need realloc() later
     anyway, so better keep the exact same behaviour. */
}
void Marker_array_drop(mut_Marker_array_p _)
{
  _->len = 0;
  _->capacity = 0;
  free(_->markers);
  _->markers = NULL;
}

mut_Marker_p Marker_array_push(mut_Marker_array_p _,
                               size_t start, size_t end, TokenType token_type)
{
  if (_->capacity < _->len + 1) {
    _->capacity *= 2;
    _->markers = realloc(_->markers, _->capacity * sizeof(Marker));
  }
  Marker_init(&(_->markers[_->len++]),
              start, end, token_type);
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
    _->markers = realloc(_->markers, _->capacity * sizeof(Marker));
  }

  size_t gap_end = position + insert_len - delete;
  memmove(_->markers + gap_end, _->markers + position + delete,
          (_->len - delete - position) * sizeof(Marker));
  _->len = _->len + insert_len - delete;
  memmove(_->markers + position, insert->array_p->markers + insert->start,
          insert_len * sizeof(Marker));
}
mut_Marker_array_p Marker_array_delete(mut_Marker_array_p _, size_t position, size_t delete)
{
  memmove(_->markers + position, _->markers + position + delete,
          (_->len - delete - position) * sizeof(Marker));
  _->len -= delete;
}
mut_Marker_array_p Marker_array_pop(mut_Marker_array_p _, mut_Marker_p item_p)
{
  if (_->len) {
    mut_Marker_p last_p = _->markers + _->len - 1;
    memmove(last_p, item_p, sizeof(Marker));
    --_->len;
  }
}

#define log(...) { fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); }


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



size_t line_number(SourceCode src, SourceCode cursor)
{
  size_t line_number = 1;
  if (cursor > src) {
    mut_SourceCode pointer = cursor;
    while (pointer is_not src) if (*(--pointer) is '\n') ++line_number;
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
  while (indent is_not 0) {
    fprintf(stderr, "  ");
    --indent;
  }
}


#define TOKEN1(token) token_type = token
#define TOKEN2(token) ++token_end;    TOKEN1(token)
#define TOKEN3(token) token_end += 2; TOKEN1(token)

int main(int argc, char** argv)
{
  log("Hello, start parsing.");

  Buffer src = read_file("test.c");
  uint8_t slice_data[80];
  Buffer slice = { 80, 0, (uint8_t *)&slice_data };

  size_t indent = 0;
  bool ignore_space = true;
  bool ignore_comment = false;

  Marker_array markers;
  Marker_array_init(&markers, 8192);

  mut_SourceCode cursor = src.bytes;
  SourceCode end = src.bytes + src.len;
  mut_SourceCode prev_cursor = 0;
  while (cursor is_not end) {
    if (cursor == prev_cursor) {
      log("ERROR: endless loop after %d.\n", cursor - src.bytes);
      break;
    }
    prev_cursor = cursor;

    TokenType token_type = T_NONE;
    mut_SourceCode token_end = NULL;
    if        ((token_end = identifier(cursor, end))) {
      TOKEN1(T_IDENTIFIER);
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
    } else if ((token_end = preprocessor(cursor, end))) {
      TOKEN1(T_PREPROCESSOR);
      if (token_end == cursor) { log("error T_PREPROCESSOR"); break; }
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
        case '.': TOKEN1(T_OP_2);        break;
        case '~': TOKEN1(T_OP_3);        break;
        case '?': TOKEN1(T_OP_16);       break;
        case '+':
          switch (c2) {
            case '+': TOKEN2(T_OP_3_2); break; // Prefix form is T_OP_3.
            case '=': TOKEN2(T_OP_16);  break;
            default:  TOKEN1(T_OP_6_3);        // Binary form is T_OP_6.
          }
          break;
        case '-':
          switch (c2) {
            case '-': TOKEN2(T_OP_3_2); break; // Prefix form is T_OP_3.
            case '=': TOKEN2(T_OP_16);  break;
            case '>': TOKEN2(T_OP_2);   break;
            default:  TOKEN1(T_OP_6_3);        // Binary form is T_OP_6.
          }
          break;
        case '*':
          switch (c2) {
            case '=': TOKEN2(T_OP_16); break;
            default:  TOKEN1(T_OP_5_3);       // Prefix form is T_OP_3.
          }
          break;
        case '/':
        case '%':
          switch (c2) {
            case '=': TOKEN2(T_OP_16); break;
            default:  TOKEN1(T_OP_5);
          }
          break;
        case '=':
          switch (c2) {
            case '=': TOKEN2(T_OP_10); break;
            default:  TOKEN1(T_OP_16);
          }
          break;
        case '!':
          switch (c2) {
            case '=': TOKEN2(T_OP_10); break;
            default:  TOKEN1(T_OP_3);
          }
          break;
        case '>':
          switch (c2) {
            case '>':
              switch (c3) {
                case '=': TOKEN3(T_OP_16); break;
                default:  TOKEN2(T_OP_7);
              }
              break;
            case '=': TOKEN3(T_OP_9); break;
            default:  TOKEN1(T_OP_9);
          }
          break;
        case '<':
          switch (c2) {
            case '<':
              switch (c3) {
                case '=': TOKEN3(T_OP_16); break;
                default:  TOKEN2(T_OP_7);
              }
              break;
            case '=': TOKEN2(T_OP_9); break;
            default:  TOKEN1(T_OP_9);
          }
          break;
        case '&':
          switch (c2) {
            case '&': TOKEN2(T_OP_14); break;
            case '=': TOKEN2(T_OP_16); break;
            default:  TOKEN1(T_OP_11_3);      // Prefix form is T_OP_3.
          }
          break;
        case '|':
          switch (c2) {
            case '|': TOKEN2(T_OP_15); break;
            case '=': TOKEN2(T_OP_16); break;
            default:  TOKEN1(T_OP_13);
          }
          break;
        case '^':
          switch (c2) {
            case '=': TOKEN2(T_OP_16); break;
            default:  TOKEN1(T_OP_12);
          }
          break;
       default: TOKEN1(T_OTHER);
      }
    }
    Marker_array_push(&markers,
                      cursor - src.bytes, token_end - src.bytes, token_type);
    cursor = token_end;
  }

  for (size_t i = 0; i is_not markers.len; ++i) {
    Marker_p m = &markers.markers[i];
    SourceCode token = extract_string(src.bytes + m->start,
                                      src.bytes + m->end,
                                      &slice);
    switch (m->token_type) {
      case T_IDENTIFIER:
        indent_log(indent);
        log("Identifier: %s", token);
        break;
      case T_NUMBER:
        indent_log(indent);
        log("Number: %s", token);
        break;
      case T_STRING:
        indent_log(indent);
        log("String: %s", token);
        break;
      case T_CHARACTER:
        indent_log(indent);
        log("Character: %s", token);
        break;
      case T_SPACE:
        if (not ignore_space) {
          indent_log(indent);
          log("Space: %s", token);
        }
        break;
      case T_COMMENT:
        if (not ignore_comment) {
          indent_log(indent);
          log("Comment: %s", token);
        }
        break;
      case T_PREPROCESSOR:
        indent_log(indent);
        log("Preprocessor: %s", token);
        break;
      case T_BLOCK_START:
        indent_log(indent); log("Start block.");
        ++indent;
        break;
      case T_BLOCK_END:
        --indent;
        indent_log(indent); log("End block.");
        break;
      case T_TUPLE_START:
        indent_log(indent); log("Start parenthetical expression.");
        ++indent;
        break;
      case T_TUPLE_END:
        --indent;
        indent_log(indent); log("End parenthetical expression.");
        break;
      case T_INDEX_START:
        indent_log(indent); log("Start subindex.");
        ++indent;
        break;
      case T_INDEX_END:
        --indent;
        indent_log(indent); log("End subindex.");
        break;
      case T_GROUP_START:
      case T_GROUP_END:
        // Invisible grouping tokens.
        break;
      case T_OP_1:  case T_OP_2:  case T_OP_3:  case T_OP_4:
      case T_OP_5:  case T_OP_6:  case T_OP_7:  case T_OP_8:
      case T_OP_9:  case T_OP_10: case T_OP_11: case T_OP_12:
      case T_OP_13: case T_OP_14: case T_OP_15: case T_OP_16:
        indent_log(indent);
        log("Operator: %s (prec. %d)", token, precedence(m->token_type));
        break;
      case T_OP_3_2:
        indent_log(indent);
        log("Operator: %s (prec. 3/2)", token);
        break;
      case T_OP_5_3:
        indent_log(indent);
        log("Operator: %s (prec. 5/3)", token);
        break;
      case T_OP_6_3:
        indent_log(indent);
        log("Operator: %s (prec. 6/3)", token);
        break;
      case T_OP_11_3:
        indent_log(indent);
        log("Operator: %s (prec. 11/3)", token);
        break;
      case T_COMMA:
        indent_log(indent); log("Comma.");
        break;
      case T_SEMICOLON:
        indent_log(indent); log("Semicolon.");
        break;
      case T_OTHER:
        indent_log(indent);
        log("OTHER: %s", token);
        break;
      default:
        log("ERROR: unexpected token type: %d %s", m->token_type, token);
        exit(25);
    }
  }

  Marker_array_drop(&markers);

  log("Read %d lines.", line_number(src.bytes, end) - 1);

  return 0;
}
