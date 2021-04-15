/// Reorganize `obj @ fn1(a), fn2(b)` as `fn1(obj, a), fn2(obj, b)`.
static void macro_backstitch(mut_Marker_array_p markers, mut_Byte_array_p src)
{
  mut_Marker_p     start  = (mut_Marker_p) Marker_array_start(markers);
  mut_Marker_mut_p cursor = start;
  mut_Marker_mut_p end    = (mut_Marker_p) Marker_array_end(markers);
  mut_Error err = { .position = 0, .message = NULL };

  Marker comma     = new_marker(src, ",", T_COMMA);
  Marker semicolon = new_marker(src, ";", T_SEMICOLON);
  Marker space     = new_marker(src, " ", T_SPACE);
  mut_Marker_array_slice object;
  mut_Marker_array_slice slice;
  while (cursor < end) {
    if (cursor->token_type is T_BACKSTITCH) {
      mut_Marker_mut_p first_segment_start = cursor + 1;
      // Trim space before first segment.
      skip_space_forward(first_segment_start, end);
      object.end_p = cursor; // Object ends before the “@”.
      // Trim space after object, between it and backstitch operator.
      skip_space_back(start, object.end_p);
      size_t nesting = 0;
      Marker_mut_p start_of_line = find_line_start(cursor, start, &err);
      if (err.message) {
        log("At line %lu: %s",
            1 + count_line_ends_between(src, 0, err.position),
            err.message);
        err.message = NULL;
      } else {
        // Trim space before object.
        skip_space_forward(start_of_line, first_segment_start);
        object.start_p = start_of_line;
        Marker object_indentation = indentation(src, object.start_p->start);
        cursor = first_segment_start;
        mut_Marker_p end_of_line = (mut_Marker_p)
            find_line_end(cursor, end, &err);
        cursor = end_of_line;
        if (err.message) {
          log("At line %lu: %s",
              count_line_ends_between(src, 0, err.position),
              err.message);
          err.message = NULL;
        } else {
          bool is_statement =
              end_of_line < end &&
              end_of_line->token_type is T_SEMICOLON;
          // TODO: check that there is a group opening before the line if
          //       if it is not an statement.
          mut_Marker_array replacement;
          // The factor of 2 here is a heuristic to avoid relocations in general.
          init_Marker_array(&replacement, 2 * (end_of_line - start_of_line));
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
                default:
                  break;
              }
              ++segment_end;
            }
            if (nesting) {
              log("Syntax error in line %lu: unclosed group.",
                  count_line_ends_between(src, 0, segment_start->start));
              return;
            }
            // Trim space after segment.
            skip_space_back(segment_start, segment_end);

            if (segment_end is segment_start) {
              log("Syntax warning in line %ld: empty backstitch segment.",
                  1 + count_line_ends_between(src, 0, segment_start->start));
              segment_end = ++segment_start;
              continue;
            }

            mut_Marker_mut_p insertion_point = segment_start;
            if (segment_start->token_type is T_INDEX_START ||
                segment_start->token_type is T_OP_1        ||
                segment_start->token_type is T_OP_14) {
              // If the segment starts with “[”, “.”, or “->”,
              // then this is already the correct insertion point.
            } else {
              // Assume function call, look for parenthesis:
              for (bool inside_parenthesis = false;
                   not inside_parenthesis and insertion_point < segment_end;
                   ++insertion_point) {
                inside_parenthesis = T_TUPLE_START is insertion_point->token_type;
              }
              // If insertion_point is segment_end, no parenthesis were found:
              // this is clearly not a function call.
              // Useful for prefix declaration like: const @ int x, float y;
            }

            if (insertion_point is segment_start) {
              splice_Marker_array(&replacement, replacement.len, 0, &object);
              if ((segment_start+1)->token_type == T_SPACE) {
                push_Marker_array(&replacement, &space);
              }
            } else if (insertion_point is segment_end) {
              insertion_point = segment_start;
              splice_Marker_array(&replacement, replacement.len, 0, &object);
              push_Marker_array(&replacement, &space);
            } else {
              slice.start_p = segment_start;
              slice.end_p   = insertion_point;
              splice_Marker_array(&replacement, replacement.len, 0, &slice);
              splice_Marker_array(&replacement, replacement.len, 0, &object);
              if (insertion_point->token_type is_not T_TUPLE_END) {
                push_Marker_array(&replacement, &comma);
                push_Marker_array(&replacement, &space);
              }
            }
            slice.start_p = insertion_point;
            slice.end_p   = segment_end;
            splice_Marker_array(&replacement, replacement.len, 0, &slice);

            if (segment_end < end_of_line) {
              if (is_statement) {
                push_Marker_array(&replacement, &semicolon);
                push_Marker_array(&replacement, &object_indentation);
              } else {
                push_Marker_array(&replacement, &comma);
                push_Marker_array(&replacement, &space);
              }
              segment_start = segment_end + 1;// One token: “,”
              // Trim space before next segment.
              skip_space_forward(segment_start, end_of_line);
              segment_end = segment_start;
            }
          }
          slice.start_p = Marker_array_start(&replacement);
          slice.end_p   = Marker_array_end  (&replacement);
          splice_Marker_array(markers,
                              start_of_line - Marker_array_start(markers),
                              end_of_line - start_of_line,
                              &slice);
          destruct_Marker_array(&replacement);
          end = (mut_Marker_p) Marker_array_end(markers);
          cursor = end_of_line + replacement.len - (end_of_line - start_of_line);
        }
      }
    }
    ++cursor;
  }
}
