/// Reorganize `obj @ fn1(a), fn2(b)` as `fn1(obj, a), fn2(obj, b)`.
static void
macro_backstitch(mut_Marker_array_p markers, mut_Byte_array_p src)
{
  mut_Marker_mut_p start  = (mut_Marker_p) Marker_array_start(markers);
  mut_Marker_mut_p cursor = start;
  mut_Marker_mut_p end    = (mut_Marker_p) Marker_array_end(markers);
  size_t cursor_position;

  mut_Error err = { .position = NULL, .message = NULL };

  Marker comma     = new_marker(src, ",",  T_COMMA);
  Marker semicolon = new_marker(src, ";",  T_SEMICOLON);
  Marker space     = new_marker(src, " ",  T_SPACE);
  Marker newline   = new_marker(src, "\n", T_SPACE);
  mut_Marker_array_slice object;
  mut_Marker_array_slice slice;
  while (cursor is_not end) {
    if (cursor->token_type is T_BACKSTITCH) {
      mut_Marker_mut_p first_segment_start = cursor + 1;
      object.end_p = cursor; // Object ends before the “@”.
      // Trim space before first segment, or affix declarator.
      skip_space_forward(first_segment_start, end);
      if (first_segment_start is end) {
        eprintln("Syntax error in line %lu: unfinished backstitch operator.",
                 line_number(src, markers, first_segment_start));
        return;
      }
      Marker_mut_p prefix = NULL, suffix = NULL;
      if (first_segment_start->token_type is T_ELLIPSIS) {
        ++first_segment_start;
        skip_space_forward(first_segment_start, end);
        if (first_segment_start is end) {
          eprintln("Syntax error in line %lu: unfinished affix declarator.",
                   line_number(src, markers, first_segment_start));
          return;
        }
        if (first_segment_start->token_type is_not T_IDENTIFIER) {
          eprintln("Syntax error in line %lu:"
                   " invalid suffix, must be an identifier.",
                   line_number(src, markers, first_segment_start));
          return;
        }
        suffix = first_segment_start++;
        skip_space_forward(first_segment_start, end);
      } else if (first_segment_start->token_type is T_IDENTIFIER) {
        mut_Marker_mut_p m = first_segment_start + 1;
        skip_space_forward(m, end);
        if (m is_not end) {
          if (m->token_type is T_ELLIPSIS) {
            prefix = first_segment_start;
            first_segment_start = m + 1;
            skip_space_forward(first_segment_start, end);
          }
        }
      }
      size_t nesting = 0;
      Marker_mut_p start_of_line = find_line_start(cursor, start, &err);
      if (err.message) {
        eprintln("Error: %lu: %s",
                 line_number(src, markers, err.position), err.message);
        err.message = NULL;
      } else {
        mut_Marker object_indentation =
            indentation(markers, start_of_line, true, src);
        if (object_indentation.token_type is T_NONE) {
          // There is no indentation because we are at the first line.
          object_indentation = newline;
        }

        // Trim space before object.
        skip_space_forward(start_of_line, object.end_p);
        // Boost precedence to 13.5, before assignment and comma operators:
        object.start_p = object.end_p;
        while (object.start_p is_not start_of_line) {
          --object.start_p;
          if (object.start_p->token_type is T_OP_14 or
              object.start_p->token_type is T_COMMA) {
            ++object.start_p;
            skip_space_forward(object.start_p, object.end_p);
            break;
          }
        }
        // Trim space after object, between it and backstitch operator.
        skip_space_back(object.start_p, object.end_p);

        cursor = first_segment_start;
        mut_Marker_mut_p end_of_line = (mut_Marker_mut_p)
            find_line_end(cursor, end, &err);
        skip_space_back(cursor, end_of_line);
        cursor = end_of_line;
        if (err.message) {
          eprintln("Error: %lu: %s",
                   line_number(src, markers, err.position), err.message);
          err.message = NULL;
        } else {
          bool ends_with_semicolon =
              end_of_line < end &&
              end_of_line->token_type is T_SEMICOLON;
          // TODO: check that there is a group opening before the line if
          //       if it is not an statement.
          mut_Marker_array replacement;
          // The factor of 2 here is a heuristic to avoid relocations in general.
          init_Marker_array(&replacement,
                            2 * (size_t)(end_of_line - object.start_p));
          mut_Marker_mut_p segment_start = first_segment_start;
          mut_Marker_mut_p segment_end   = segment_start;
          while (segment_end < end_of_line) {
            // Look for segment end.
            while (segment_end < end_of_line) {
              if (not nesting and segment_end->token_type is T_COMMA) break;
              switch (segment_end->token_type) {
                case T_BLOCK_START: case T_TUPLE_START: case T_INDEX_START:
                  ++nesting;
                  break;
                case T_BLOCK_END: case T_TUPLE_END: case T_INDEX_END:
                  --nesting;// Can not underflow because of find_line_end().
                  break;
                case T_ELLIPSIS:
                  eprintln("Syntax error in line %lu:"
                           " invalid prefix, must be an identifier.",
                           line_number(src, markers, segment_end));
                  break;
                default:
                  break;
              }
              ++segment_end;
            }
            if (nesting) {
              eprintln("Error: %lu: unclosed group, syntax error.",
                       line_number(src, markers, segment_start));
              destruct_Marker_array(&replacement);
              return;
            }
            // Trim space after segment.
            skip_space_back(segment_start, segment_end);

            if (segment_end is segment_start) {
              eprintln("Warning: %ld: empty backstitch segment.",
                       line_number(src, markers, segment_start));
              segment_end = ++segment_start;
              continue;
            }

            mut_Marker_mut_p insertion_point = segment_start;
            bool inside_parenthesis = false;
            bool is_function_call   = true;
            if (segment_start->token_type is T_INDEX_START ||
                segment_start->token_type is T_OP_1        ||
                segment_start->token_type is T_OP_14       ||
                object.end_p is object.start_p) {
              // If the segment starts with “[”, “.”, “->”, or “=”,
              // then this is already the correct insertion point.
            } else {
              // Assume function call, look for parenthesis:
              while (not inside_parenthesis and insertion_point < segment_end) {
                TokenType t = insertion_point->token_type;
                if (T_IDENTIFIER is t) {
                  is_function_call = true;
                } else if (is_keyword(t)) {
                  is_function_call = false;
                } else if (T_TUPLE_START is t) {
                  if (insertion_point is_not start and is_function_call) {
                    // Ignore control flow statements like if(...) etc.
                    inside_parenthesis = true;
                  }
                } else if (T_BLOCK_START is t or T_OP_13 is t) {
                  break;
                }
                ++insertion_point;
              }
              // If insertion_point is segment_end, no parenthesis were found:
              // this is clearly not a function call.
              // Useful for prefix declaration like: const @ int x, float y;
            }
            if (insertion_point is segment_end) {
              insertion_point = segment_start;
            }

            if (insertion_point is segment_start) {
              if (object.start_p is_not object.end_p) {
                splice_Marker_array(&replacement, replacement.len, 0, NULL,
                                    &object);
                if ((segment_start+1)->token_type == T_SPACE) {
                  push_Marker_array(&replacement, space);
                }
              }
              if (prefix) push_Marker_array(&replacement, *prefix);
              if (suffix) {
                push_Marker_array(&replacement, *insertion_point++);
                push_Marker_array(&replacement, *suffix);
              }
            } else {
              slice.start_p = segment_start;
              slice.end_p   = insertion_point;
              if (prefix || suffix) {
                while (slice.end_p != segment_start) {
                  --slice.end_p;
                  if (slice.end_p->token_type is T_IDENTIFIER) break;
                }
                splice_Marker_array(&replacement, replacement.len, 0, NULL,
                                    &slice);
                if (prefix) {
                  push_Marker_array(&replacement, *prefix);
                } else {
                  push_Marker_array(&replacement, *slice.end_p++);
                  push_Marker_array(&replacement, *suffix);
                }
                slice.start_p = slice.end_p;
                slice.end_p   = insertion_point;
              }
              splice_Marker_array(&replacement, replacement.len, 0, NULL,
                                  &slice);
              // Only insert object if not empty, allow elided object.
              if (object.end_p is_not object.start_p) {
                splice_Marker_array(&replacement, replacement.len, 0, NULL,
                                    &object);
                if (inside_parenthesis) {
                  if (insertion_point->token_type is_not T_TUPLE_END) {
                    push_Marker_array(&replacement, comma);
                    push_Marker_array(&replacement, space);
                  }
                } else {
                  push_Marker_array(&replacement, space);
                }
              }
            }
            slice.start_p = insertion_point;
            slice.end_p   = segment_end;
            splice_Marker_array(&replacement, replacement.len, 0, NULL,
                                &slice);

            if (segment_end < end_of_line) {
              if (ends_with_semicolon) {
                push_Marker_array(&replacement, semicolon);
                push_Marker_array(&replacement, object_indentation);
              } else {
                push_Marker_array(&replacement, comma);
                push_Marker_array(&replacement, space);
              }
              segment_start = segment_end + 1;// One token: “,”
              // Trim space before next segment.
              skip_space_forward(segment_start, end_of_line);
              segment_end = segment_start;
            }
          }
          slice.start_p = Marker_array_start(&replacement);
          slice.end_p   = Marker_array_end  (&replacement);
          // Invalidates: markers
          cursor_position = (size_t)(object.start_p - start);
          splice_Marker_array(markers,
                              cursor_position,
                              (size_t)(end_of_line - object.start_p),
                              NULL,
                              &slice);
          cursor_position += replacement.len;
          start = (mut_Marker_p)Marker_array_start(markers);
          end   = (mut_Marker_p)Marker_array_end  (markers);
          cursor = start + cursor_position;
          slice.start_p = slice.end_p = NULL;
          destruct_Marker_array(&replacement);
        }
      }
    }
    ++cursor;
  }
}
