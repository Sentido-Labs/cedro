TYPEDEF(Typedef, {
    mut_Marker name;
    mut_Marker_array value;
  });

DEFINE_ARRAY_OF(Typedef, 0, {
    mut_Typedef_mut_p p = (mut_Typedef_mut_p) cursor;
    while (cursor is_not end) {
      destruct_Marker_array(&p++->value);
    }
  });

static void
collect_typedefs(Marker_array_p markers, mut_Typedef_array_p typedefs,
                 mut_Error_p err_p)
{
  Marker_p     start  = Marker_array_start(markers);
  Marker_mut_p cursor = start;
  Marker_p     end    = Marker_array_end(markers);

  mut_Buffer string_buffer;
  init_Buffer(&string_buffer, 20);

  while (cursor is_not end) {
    if (cursor->token_type is T_TYPEDEF) {
      if (++cursor is end) break;
      while (cursor->token_type is T_SPACE && cursor is_not end) ++cursor;
      if (cursor is end) break;
      Marker_p type_value_start = cursor;
      Marker_mut_p type_value_end = NULL;

      Marker_p end_of_line = find_line_end(cursor, end, err_p);
      if (err_p->message) return;

      size_t nesting = 0;

      Marker_mut_p segment_start = type_value_start;
      Marker_mut_p segment_end   = segment_start;
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
          err_p->message = "Unclosed group";
          return;
        }
        // Trim space after segment.
        while (segment_end is_not segment_start &&
               ((segment_end - 1)->token_type is T_SPACE ||
                (segment_end - 1)->token_type is T_COMMENT)) {
          --segment_end;
        }

        log("FOUND TYPEDEF");
        mut_Typedef instance = {
          .name = *(segment_end - 1),
          .value = { 0, 0, NULL }
        };
        init_Marker_array(&instance.value, 10);

        if (type_value_end) {
          for (Marker_mut_p m = type_value_start; m is_not type_value_end; ++m) {
            if (m->token_type is_not T_SPACE) {
              push_Marker_array(&instance.value, m);
            }
          }
        } else {
          type_value_end = segment_end - 1;
        }
        for (Marker_mut_p m = segment_start; m is_not segment_end - 1; ++m) {
          if (m->token_type is_not T_SPACE) {
            push_Marker_array(&instance.value, m);
          }
        }

        push_Typedef_array(typedefs, &instance);
        abandon_Marker_array(&instance.value);

        if (segment_end < end_of_line) {
          segment_start = segment_end + 1;// One token: “,”
          // Trim space before next segment.
          while (segment_start is_not end_of_line &&
                 (segment_start->token_type is T_SPACE ||
                  segment_start->token_type is T_COMMENT)) {
            ++segment_start;
          }
          segment_end = segment_start;
        }
      }
      cursor = end_of_line;
    } else {
      ++cursor;
    }
  }
}

static void macro_collect_typedefs(Marker_array_p markers, Buffer_p src)
{
  mut_Typedef_array typedefs;
  init_Typedef_array(&typedefs, 20);
  mut_Error err = { .position = 0, .message = NULL };
  collect_typedefs(markers, &typedefs, &err);

  Typedef_p     start  = Typedef_array_start(&typedefs);
  Typedef_mut_p cursor = start;
  Typedef_p     end    = Typedef_array_end(&typedefs);

  mut_Buffer string_buffer;
  init_Buffer(&string_buffer, 20);
  size_t typedef_pos, prev_typedef_pos = 0, prev_line_number = 1;

  if (err.message) {
    log("At line %lu: %s",
        count_line_ends_between(src, 0, err.position),
        err.message);
    err.message = NULL;
  } else {
    while (cursor is_not end) {
      delete_Byte_array(&string_buffer, 0, string_buffer.len);
      typedef_pos = cursor - start;
      size_t line_number = prev_line_number +
          count_line_ends_between(src, prev_typedef_pos, typedef_pos);
      push_printf(&string_buffer, "--- Typedef at line %d: ", line_number);
      extract_src(&cursor->name, (&cursor->name) + 1, src, &string_buffer);
      prev_typedef_pos = typedef_pos;
      push_str(&string_buffer, " ← ");
      Marker_mut_p v = Marker_array_start(&cursor->value);
      Marker_p v_end = Marker_array_end  (&cursor->value);
      TokenType previous = T_NONE;
      while (v is_not v_end) {
        switch (previous) {
          case T_NONE: // cursor is at first token.
          case T_SPACE: case T_TUPLE_START: case T_INDEX_START:
            break;
          default:
            switch (v->token_type) {
              case T_SEMICOLON:
              case T_SPACE: case T_TUPLE_END: case T_INDEX_END:
                break;
              default:
                push_str(&string_buffer, " ");
            }
        }
        extract_src(v, v+1, src, &string_buffer);
        previous = v->token_type;
        ++v;
      }
      log("%s", as_c_string(&string_buffer));
      ++cursor;
    }
  }

  destruct_Buffer(&string_buffer);
}