/// Because of the lack of fences, it works for
/// structs and array initializers, and for separate function arguments.
/// x[a..b] → &x[a], &x[b]
/// x[a..+b] → &x[a], &x[a+b]
/// Uses &x[a] in preference to x+a as advised in page 111 of
/// “21st Century C” by Ben Klemens.
static void
macro_slice(mut_Marker_array_p markers, mut_Byte_array_p src)
{
  Marker_mut_p start  = start_of_Marker_array(markers);
  Marker_mut_p cursor = start;
  Marker_mut_p end    = end_of_Marker_array(markers);

  mut_Error err = { .position = NULL, .message = NULL };

  Marker comma            = Marker_from(src, ",", T_COMMA);
  Marker space            = Marker_from(src, " ", T_SPACE);
  Marker  openBrackets    = Marker_from(src, "[", T_INDEX_START);
  Marker closeBrackets    = Marker_from(src, "]", T_INDEX_END);
  Marker  openParenthesis = Marker_from(src, "(", T_INDEX_START);
  Marker closeParenthesis = Marker_from(src, ")", T_INDEX_END);
  Marker addressOf        = Marker_from(src, "&", T_OP_2);

  mut_Marker_array replacement;
  init_Marker_array(&replacement, 30);

  while (cursor is_not end) {
    if (cursor->token_type is T_ELLIPSIS and cursor->len is 2) {
      Marker_array_mut_slice a = {
        .start_p = find_line_start(cursor, start, &err),
        .end_p = cursor
      };
      if (err.message) break;
      Marker_array_mut_slice b = {
        .start_p = cursor + 1,
        .end_p = find_line_end(cursor, end, &err),
      };
      if (err.message) break;
      if (a.start_p > start+2 and b.end_p is_not end and
          (a.start_p-1)->token_type is T_INDEX_START and
          (b.end_p    )->token_type is T_INDEX_END) {
        Marker_array_mut_slice array = {
          .start_p = a.start_p-1,
          .end_p   = a.start_p-1
        };
        size_t nesting = 0;
        while (array.start_p is_not start) {
          switch ((array.start_p - 1)->token_type) {
            case T_BLOCK_START: case T_TUPLE_START: case T_INDEX_START:
              if (not nesting) goto found_pointer_expression_start;
              --nesting;
              break;
            case T_BLOCK_END: case T_TUPLE_END: case T_INDEX_END:
              ++nesting;
              break;
            case T_COMMA: case T_SEMICOLON: case T_BACKSTITCH:
              if (not nesting) goto found_pointer_expression_start;
              break;
            case T_OP_14: /* = += -= *= /= %= <<= >>= &= ^= |= */
              if (not nesting) {
                error_at(LANG("esta porción necesita llaves {...} alrededor",
                              "this slice needs braces {...} around it"),
                         array.start_p, markers, src);
                goto exit;
              }
              break;
            default:
              break;
          }
          --array.start_p;
        } found_pointer_expression_start:

        a.start_p = skip_space_forward(a.start_p, a.end_p);
        a.end_p   = skip_space_back   (a.start_p, a.end_p);
        b.start_p = skip_space_forward(b.start_p, b.end_p);
        b.end_p   = skip_space_back   (b.start_p, b.end_p);
        array.start_p = skip_space_forward(array.start_p, array.end_p);
        array.end_p   = skip_space_back   (array.start_p, array.end_p);
        bool array_is_expression = array.end_p - array.start_p > 1;
        replacement.len = 0;
        push_Marker_array(&replacement, addressOf);
        if (array_is_expression) {
          push_Marker_array(&replacement, openParenthesis);
          append_Marker_array(&replacement, array);
          push_Marker_array(&replacement, closeParenthesis);
        } else {
          append_Marker_array(&replacement, array);
        }
        push_Marker_array(&replacement, openBrackets);
        append_Marker_array(&replacement, a);
        push_Marker_array(&replacement, closeBrackets);
        push_Marker_array(&replacement, comma);
        push_Marker_array(&replacement, space);
        push_Marker_array(&replacement, addressOf);
        if (array_is_expression) {
          push_Marker_array(&replacement, openParenthesis);
          append_Marker_array(&replacement, array);
          push_Marker_array(&replacement, closeParenthesis);
        } else {
          append_Marker_array(&replacement, array);
        }
        push_Marker_array(&replacement, openBrackets);
        if (b.start_p->token_type is T_OP_2 and
            b.start_p->len is 1 and
            src->start[b.start_p->start] is '+') {
          append_Marker_array(&replacement, a);
          if (b.start_p + 1 < b.end_p and b.start_p[1].token_type is T_SPACE) {
            push_Marker_array(&replacement, b.start_p[1]);
          }
        }
        append_Marker_array(&replacement, b);
        push_Marker_array(&replacement, closeBrackets);
        // Invalidates: markers
        size_t cursor_position = (size_t)(array.start_p - start);
        splice_Marker_array(markers, cursor_position,
                            (size_t)(b.end_p + 1 - array.start_p), NULL,
                            bounds_of_Marker_array(&replacement));
        cursor_position += replacement.len;
        start = start_of_Marker_array(markers);
        end   =   end_of_Marker_array(markers);
        cursor = start + cursor_position;
      }
    }
    ++cursor;
  } exit:

  destruct_Marker_array(&replacement);

  if (err.message) {
    eprintln("Error: %lu: %s",
             line_number(src, markers, err.position), err.message);
    err.message = NULL;
  }
}
