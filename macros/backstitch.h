/// Reorganize `obj::fn1(a)::fn2(b)` as `fn1(obj, a), fn2(obj, b)`.
void macro_backstitch(mut_Marker_array_p markers, mut_Buffer_p src)
{
  Marker comma = new_marker(src, ",", T_COMMA);
  Marker space = new_marker(src, " ", T_SPACE);
  mut_Marker_p     start  = (mut_Marker_p) Marker_array_start(markers);
  mut_Marker_mut_p cursor = start + 1;
  mut_Marker_mut_p end    = (mut_Marker_p) Marker_array_end(markers);
  mut_Marker_array_slice object;
  mut_Marker_array_slice slice;
  while (cursor < end) {
    if (cursor->token_type == T_BACKSTITCH) {
      mut_Marker_mut_p first_call_start = cursor + 1;
      // Trim space before first segment.
      while (first_call_start != end &&
             (first_call_start->token_type == T_SPACE ||
              first_call_start->token_type == T_COMMENT)) {
        ++first_call_start;
      }
      object.end_p = cursor; // Object ends before the “@”.
      // Trim space after object, between it and backstitch operator.
      while (object.end_p != start &&
             ((object.end_p - 1)->token_type == T_SPACE ||
              (object.end_p - 1)->token_type == T_COMMENT)) {
        --object.end_p;
      }
      size_t nesting = 0;
      mut_Marker_mut_p start_of_line = NULL;
      while (not start_of_line and cursor >= start) {
        switch (cursor->token_type) {
          case T_SEMICOLON:
            start_of_line = cursor + 1;
            break;
          case T_BLOCK_START: case T_TUPLE_START: case T_INDEX_START:
            if (nesting is 0) start_of_line = cursor + 1;
            else              --nesting;
            break;
          case T_BLOCK_END: case T_TUPLE_END: case T_INDEX_END:
            ++nesting;
            break;
          default: break;
        }
        --cursor;
      }
      assert(!!start_of_line == !nesting);
      if (nesting) {
        log("Unclosed group in line %lu",
            line_number(src, start_of_line->start));
        return;
      }
      // Trim space before object.
      while (start_of_line != first_call_start &&
             (start_of_line->token_type == T_SPACE ||
              start_of_line->token_type == T_COMMENT)) {
        ++start_of_line;
      }
      object.start_p = start_of_line;
      cursor = first_call_start;
      mut_Marker_mut_p end_of_line = NULL;
      while (not end_of_line and cursor < end) {
        switch (cursor->token_type) {
          case T_SEMICOLON:
            end_of_line = cursor;
            break;
          case T_BLOCK_START: case T_TUPLE_START: case T_INDEX_START:
            ++nesting;
            break;
          case T_BLOCK_END: case T_TUPLE_END: case T_INDEX_END:
            if (nesting is 0) end_of_line = cursor;
            else              --nesting;
            break;
          default: break;
        }
        ++cursor;
      }
      if (nesting) {
        log("Unclosed group in line %lu",
            line_number(src, first_call_start->start));
        return;
      }
      if (end_of_line) {
        mut_Marker_array replacement;
        // The factor of 2 here is a heuristic to avoid relocations in general.
        init_Marker_array(&replacement, 2 * (end_of_line - start_of_line));
        mut_Marker_mut_p segment_start = first_call_start;
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
                --nesting;// Can not underflow because of the previous loop.
                break;
              default:
                break;
            }
            ++segment_end;
          }
          if (nesting) {
            log("Unclosed group in line %lu",
                line_number(src, segment_start->start));
            return;
          }
          // Trim space after segment.
          while (segment_end != segment_start &&
                 ((segment_end - 1)->token_type == T_SPACE ||
                  (segment_end - 1)->token_type == T_COMMENT)) {
            --segment_end;
          }

          mut_Marker_mut_p insertion_point = segment_start;
          if (segment_start->token_type == T_INDEX_START ||
              segment_start->token_type == T_OP_1 ||
              segment_start->token_type == T_OP_14) {
            // If the segment starts with “[”, “.”, or “->”,
            // then this is already the correct insertion point.
          } else {
            // Default case, a function call.
            for (bool inside_parenthesis = false;
                 not inside_parenthesis && insertion_point < segment_end;
                 ++insertion_point) {
              inside_parenthesis = T_TUPLE_START == insertion_point->token_type;
            }
          }

          slice.start_p = segment_start;
          slice.end_p   = insertion_point;
          splice_Marker_array(&replacement, replacement.len, 0, &slice);
          splice_Marker_array(&replacement, replacement.len, 0, &object);
          if (insertion_point != segment_start &&
              /*insertion_point != segment_end // Never happens anyway*/
              insertion_point->token_type != T_TUPLE_END) {
            push_Marker_array(&replacement, &comma);
            push_Marker_array(&replacement, &space);
          }
          slice.start_p = insertion_point;
          slice.end_p   = segment_end;
          splice_Marker_array(&replacement, replacement.len, 0, &slice);

          if (segment_end < end_of_line) {
            push_Marker_array(&replacement, &comma);
            push_Marker_array(&replacement, &space);
            segment_start = segment_end + 1;// One token: “,”
            // Trim space before next segment.
            while (segment_start != end_of_line &&
                   (segment_start->token_type == T_SPACE ||
                    segment_start->token_type == T_COMMENT)) {
              ++segment_start;
            }
            segment_end = segment_start;
          }
        }
        slice.start_p = Marker_array_start(&replacement);
        slice.end_p   = Marker_array_end  (&replacement);
        splice_Marker_array(markers,
                            start_of_line - Marker_array_start(markers),
                            end_of_line - start_of_line,
                            &slice);
        drop_Marker_array(&replacement);
        end = (mut_Marker_p) Marker_array_end(markers);
        cursor = end_of_line + replacement.len - (end_of_line - start_of_line);
      } else {
        log("Error: unterminated backstitch expression, started at line %ld",
            line_number(src, first_call_start->start));
      }
    }
    ++cursor;
  }
}