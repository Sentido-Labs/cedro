/// Reorganize `obj @ fn1(a), fn2(b)` as `fn1(obj, a), fn2(obj, b)`.
static void
macro_backstitch(mut_Marker_array_p markers, mut_Byte_array_p src)
{
  Marker_mut_p start  = start_of_Marker_array(markers);
  Marker_mut_p cursor = start;
  Marker_mut_p end    = end_of_Marker_array(markers);
  size_t cursor_position;

  mut_Error err = { .position = NULL, .message = NULL };

  Marker comma     = Marker_from(src, ",",  T_COMMA);
  Marker semicolon = Marker_from(src, ";",  T_SEMICOLON);
  Marker space     = Marker_from(src, " ",  T_SPACE);
  Marker newline   = Marker_from(src, "\n", T_SPACE);
  Marker_array_mut_slice object;
  Marker_array_mut_slice slice;
  while (cursor is_not end) {
    if (cursor->token_type is T_BACKSTITCH) {
      Marker_mut_p first_segment_start = cursor + 1;
      object.end_p = cursor; // Object ends before the “@”.
      // Trim space before first segment, or affix declarator.
      first_segment_start = skip_space_forward(first_segment_start, end);
      if (first_segment_start is end) {
        error_at(LANG("macro pespunte incompleto.",
                      "unfinished backstitch macro."),
                 cursor, markers, src);
        return;
      }
      Marker_mut_p prefix = NULL, suffix = NULL;
      if (first_segment_start->token_type is T_ELLIPSIS) {
        first_segment_start = skip_space_forward(first_segment_start + 1, end);
        if (first_segment_start is end) {
          error_at(LANG("declarador (pre|su)fijo incompleto.",
                        "unfinished affix declarator."),
                   cursor, markers, src);
          return;
        }
        if (first_segment_start->token_type is_not T_IDENTIFIER) {
          error_at(LANG("prefijo no válido, debe ser un identificador.",
                        "invalid suffix, must be an identifier."),
                   cursor, markers, src);
          return;
        }
        suffix = first_segment_start++;
        first_segment_start = skip_space_forward(first_segment_start, end);
      } else if (first_segment_start->token_type is T_IDENTIFIER) {
        Marker_mut_p m = first_segment_start + 1;
        m = skip_space_forward(m, end);
        if (m is_not end) {
          if (m->token_type is T_ELLIPSIS) {
            prefix = first_segment_start;
            first_segment_start = skip_space_forward(m + 1, end);
          }
        }
      }
      Marker_mut_p start_of_line = find_line_start(cursor, start, &err);
      if (err.message) {
        error_at(err.message, err.position, markers, src);
        err.message = NULL;
      } else {
        mut_Marker object_indentation =
            indentation(markers, start_of_line, true, src);
        if (object_indentation.token_type is T_NONE) {
          // There is no indentation because we are at the first line.
          object_indentation = newline;
        }

        // Trim space before object.
        start_of_line = skip_space_forward(start_of_line, object.end_p);
        // Boost precedence to 13.5, before assignment and comma operators:
        size_t nesting = 0;
        object.start_p = object.end_p;
        while (object.start_p is_not start_of_line) {
          --object.start_p;
          switch (object.start_p->token_type) {
            case T_TUPLE_END: case T_INDEX_END: case T_GROUP_END:
              ++nesting;
              break;
            case T_TUPLE_START: case T_INDEX_START: case T_GROUP_START:
              // nesting can not be 0: find_line_start() checks fence pairing.
              assert(nesting is_not 0);
              --nesting;
              break;
            case T_OP_14: case T_COMMA:
              if (nesting is 0) {
                object.start_p = skip_space_forward(object.start_p + 1,
                                                    object.end_p);
                goto boosted_precedence;
              }
              break;
            default:
              break;
          }
        } boosted_precedence:
        // Trim space after object, between it and the backstitch operator.
        object.end_p = skip_space_back(object.start_p, object.end_p);

        Marker_mut_p end_of_line =
            find_line_end(first_segment_start, end, &err);
        if (err.message) {
          error_at(err.message, err.position, markers, src);
          err.message = NULL;
        } else {
          end_of_line = skip_space_back(first_segment_start, end_of_line);
          bool ends_with_semicolon =
              end_of_line < end and
              end_of_line->token_type is T_SEMICOLON;
          bool empty_object = object.start_p is object.end_p;
          bool empty_segments = first_segment_start is end_of_line;
          if (empty_object and empty_segments) {
            error_at(LANG("no se puede omitir a la vez"
                          " el objeto de pespunte y los segmentos.",
                          "backstitch object and segments"
                          " can not be both omitted at the same time."),
                     cursor, markers, src);
            return;
          }

          // TODO: check that there is a group opening before the line if
          //       if it is not an statement.
          mut_Marker_array replacement;
          // The factor of 2 here is a heuristic to avoid relocations.
          init_Marker_array(&replacement,
                            2 * (size_t)(end_of_line - object.start_p));
          Marker_mut_p segment_start = first_segment_start;
          Marker_mut_p segment_end   = segment_start;
          do {
            size_t empty_lines_after_segment = 0;
            // Look for segment end.
            while (segment_end < end_of_line) {
              switch (segment_end->token_type) {
                case T_COMMA:
                  if (not nesting) {
                    if (segment_end + 1 is_not end_of_line) {
                      empty_lines_after_segment =
                          count_appearances('\n',
                                            segment_end + 1, segment_end + 2,
                                            src);
                      if (empty_lines_after_segment) {
                        --empty_lines_after_segment;
                      }
                    }
                    goto found_segment_end;
                  }
                  break;
                case T_BLOCK_START: case T_TUPLE_START: case T_INDEX_START:
                  ++nesting;
                  break;
                case T_BLOCK_END: case T_TUPLE_END: case T_INDEX_END:
                  --nesting;// Can not underflow because of find_line_end().
                  break;
                case T_ELLIPSIS:
                  error_at(LANG("prefijo no válido, debe ser un identificador.",
                                "invalid prefix, must be an identifier."),
                           cursor, markers, src);
                  destruct_Marker_array(&replacement);
                  return;
                  //break;
                default:
                  break;
              }
              ++segment_end;
            } found_segment_end:
            if (nesting) {
              error_at(LANG("error sintáctico, grupo sin cerrar.",
                            "unclosed group, syntax error."),
                       cursor, markers, src);
              destruct_Marker_array(&replacement);
              return;
            }
            // Trim space after segment.
            segment_end = skip_space_back(segment_start, segment_end);

            Marker_mut_p insertion_point = segment_start;
            bool inside_parenthesis = false;
            bool is_function_call   = true;
            if (segment_start->token_type is T_INDEX_START or
                segment_start->token_type is T_OP_1        or
                segment_start->token_type is T_OP_14) {
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

            slice.start_p = segment_start;
            slice.end_p   = insertion_point;
            if (prefix or suffix) {
              while (slice.end_p is_not slice.start_p) {
                --slice.end_p;
                if (slice.end_p->token_type is T_IDENTIFIER) break;
              }
              append_Marker_array(&replacement, slice);
              if (empty_segments) {
                // Modifying object is not a problem because there are
                // no further segments, so we won’t use it again.
                while (object.start_p->token_type is T_OP_2 and
                       object.start_p is_not cursor) ++object.start_p;
                if (object.start_p->token_type is_not T_IDENTIFIER) {
                  error_at(LANG("el (pseudo-)objeto debe empezar"
                                " con un identificador.",
                                "the (pseudo-)object must start"
                                " with an identifier."),
                           cursor, markers, src);
                  destruct_Marker_array(&replacement);
                  return;
                }
                if (prefix) {
                  push_Marker_array(&replacement, *prefix);
                } else { // suffix
                  object.start_p++;
                  insertion_point = slice.end_p;
                  push_Marker_array(&replacement, *suffix);
                }
              } else {
                if (prefix) {
                  push_Marker_array(&replacement, *prefix);
                } else {
                  push_Marker_array(&replacement, *slice.end_p++);
                  if (empty_object) insertion_point = slice.end_p;
                  push_Marker_array(&replacement, *suffix);
                }
              }
              slice.start_p = slice.end_p;
              slice.end_p   = insertion_point;
              if (slice.start_p > slice.end_p) {
                error_at(LANG("falta el objeto.",
                              "missing object."),
                         cursor, markers, src);
                destruct_Marker_array(&replacement);
                return;
              }
            }
            append_Marker_array(&replacement, slice);
            // Only insert object if not empty, allow elided object.
            if (not empty_object) {
              append_Marker_array(&replacement, object);
              if (inside_parenthesis) {
                if (insertion_point->token_type is_not T_TUPLE_END) {
                  push_Marker_array(&replacement, comma);
                  push_Marker_array(&replacement, space);
                }
              } else if (segment_start is_not end_of_line) {
                TokenType object_end = cursor is_not start?
                    (cursor-1)->token_type: T_NONE;
                if (object_end is T_SPACE or
                    (object_end is T_NUMBER or
                     object_end is T_IDENTIFIER) and
                    (segment_start->token_type is T_NUMBER or
                     segment_start->token_type is T_IDENTIFIER)) {
                  push_Marker_array(&replacement, space);
                }
              }
            }

            slice.start_p = insertion_point;
            slice.end_p   = segment_end;
            append_Marker_array(&replacement, slice);

            if (segment_end < end_of_line) {
              if (ends_with_semicolon) {
                push_Marker_array(&replacement, semicolon);
                for (size_t i = 0; i is_not empty_lines_after_segment; ++i) {
                  push_Marker_array(&replacement, newline);
                }
                push_Marker_array(&replacement, object_indentation);
              } else {
                push_Marker_array(&replacement, comma);
                push_Marker_array(&replacement, space);
              }
              segment_start = segment_end + 1;// One token: “,”
              // Trim space before next segment.
              segment_start = skip_space_forward(segment_start, end_of_line);
              segment_end = segment_start;
            }
          } while (segment_end < end_of_line);
          // Invalidates: markers
          cursor_position = (size_t)(object.start_p - start);
          splice_Marker_array(markers,
                              cursor_position,
                              (size_t)(end_of_line - object.start_p),
                              NULL,
                              bounds_of_Marker_array(&replacement));
          cursor_position += replacement.len;
          start = start_of_Marker_array(markers);
          end   =   end_of_Marker_array(markers);
          cursor = start + cursor_position;
          destruct_Marker_array(&replacement);
        }
      }
    }
    ++cursor;
  }
}
