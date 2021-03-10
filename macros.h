void macro_count_markers(mut_Marker_array_p markers, Buffer_p src)
{
  fprintf(stderr, "Markers: %ld\n", markers->len);
}

/// Reorganize `obj::fn1(a)::fn2(b)` as `fn1(obj, a), fn2(obj, b)`.
void macro_backstitch(mut_Marker_array_p markers, mut_Buffer_p src)
{
  Marker comma = new_marker(src, ",", T_COMMA);
  mut_Marker_p     start  = (mut_Marker_p) Marker_array_start(markers);
  mut_Marker_mut_p cursor = start + 1;
  mut_Marker_p     end    = (mut_Marker_p) Marker_array_end(markers);
  mut_Marker_array_slice object;
  mut_Marker_array_slice slice;
  TokenType previous = T_NONE;
  while (cursor < end) {
    if (cursor->token_type == T_OP_13 && previous == T_OP_13) {
      mut_Marker_p first_call_start = cursor + 1;
      object.end = (cursor - 1 - start);
      object.array_p = markers;
      size_t nesting = 0;
      cursor = first_call_start - 3;// Before the “::”.
      mut_Marker_mut_p start_of_line = NULL;
      while (not start_of_line and cursor >= start) {
        switch (cursor->token_type) {
          case T_SEMICOLON: start_of_line = cursor + 1; break;
          case T_BLOCK_START: case T_TUPLE_START: case T_INDEX_START:
            if (nesting is 0) start_of_line = cursor + 1;
            else              --nesting;
            break;
          case T_BLOCK_END: case T_TUPLE_END: case T_INDEX_END:
            ++nesting; break;
          default: break;
        }
        --cursor;
      }
      while (start_of_line->token_type == T_SPACE) ++start_of_line;
      object.start = start_of_line - start;
      cursor = first_call_start;
      mut_Marker_mut_p end_of_line = NULL;
      while (not end_of_line and cursor < end) {
        switch (cursor->token_type) {
          case T_SEMICOLON: end_of_line = cursor; break;
          case T_BLOCK_START: case T_TUPLE_START: case T_INDEX_START:
            ++nesting; ++cursor; break;
          case T_BLOCK_END: case T_TUPLE_END: case T_INDEX_END:
            if (nesting is 0) end_of_line = cursor;
            else              --nesting;
            break;
          default: break;
        }
        ++cursor;
      }
      if (end_of_line) {
        mut_Marker_array replacement;
        init_Marker_array(&replacement, 2 * (end_of_line - start_of_line));
        mut_Marker_mut_p segment_start = first_call_start;
        mut_Marker_mut_p segment_end   = segment_start;
        while (segment_end < end_of_line) {
          // Look for segment end.
          while (segment_end < end_of_line) {
            if (not nesting and segment_end->token_type is T_OP_13) break;
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
          push_Marker_array(&replacement, segment_start);
          push_Marker_array(&replacement, segment_start + 1);
          splice_Marker_array(&replacement, replacement.len, 0, &object);
          push_Marker_array(&replacement, &comma);
          slice.start = (segment_start + 2 - start);
          slice.end   = (segment_end       - start);
          slice.array_p = markers;
          splice_Marker_array(&replacement, replacement.len, 0, &slice);
          if (segment_end < end_of_line) {
            push_Marker_array(&replacement, &comma);
            segment_start = segment_end + 2;// Two tokens: “::”
            segment_end = segment_start;
          }
        }
        slice.start = 0;
        slice.end   = replacement.len;
        slice.array_p = &replacement;
        splice_Marker_array(markers,
                            start_of_line - Marker_array_start(markers),
                            end_of_line - start_of_line,
                            &slice);
        drop_Marker_array(&replacement);
      }
      previous = cursor->token_type;
      ++cursor;
    } else {
      previous = cursor->token_type;
      ++cursor;
    }
  }
}

void macro_function_pointer(mut_Marker_array_p markers, Buffer_p src)
{
  fprintf(stderr, "TODO: reorganize function_pointer(void fname(arg1, arg2)) as void (*fname)(arg1, arg2)\n");
}

void macro_fn(mut_Marker_array_p markers, Buffer_p src)
{
  fprintf(stderr, "TODO: reorganize fn fname(arg1: type1, arg2: mut type2) -> void {...} as void fname(const type1 arg1, type2 arg2) {...}\n");
}

void macro_let(mut_Marker_array_p markers, Buffer_p src)
{
  fprintf(stderr, "TODO: reorganize let x: int = 3; as int x = 3;\n");
}
