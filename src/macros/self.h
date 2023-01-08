/* -*- coding: utf-8 c-basic-offset: 2 tab-width: 2 indent-tabs-mode: nil -*-
 * vi: set et ts=2 sw=2: */
/// Reorganize `obj->fn(a); obj.fn(b);` as `obj->fn(obj, a); obj.fn(&obj, b);`.
static void
macro_self(mut_Marker_array_p markers, mut_Byte_array_p src,
           Options options)
{
  if (not options.pass_self_to_member_functions) return;

  Marker_mut_p start  = start_of_Marker_array(markers);
  Marker_mut_p cursor = start;
  Marker_mut_p end    = end_of_Marker_array(markers);
  size_t cursor_position;

  Marker comma     = Marker_from(src, ",",  T_COMMA);
  Marker space     = Marker_from(src, " ",  T_SPACE);
  Marker addressOf = Marker_from(src, "&", T_OP_2);
  Marker void_cast = Marker_from(src, "(void*)", T_OP_1);

  mut_Marker_array replacement = init_Marker_array(30);

  mut_Error err = { .position = NULL, .message = NULL };

  while (cursor is_not end) {
    if (cursor->token_type is T_OP_1 and cursor is_not start) {
      Byte_p c = &src->start[cursor->start];
      bool value_member   = cursor->len is 1 and *c is '.';
      bool pointer_member = cursor->len is 2 and *c is '-' && *(c+1) is '>';
      if (cursor + 1 is_not end and (value_member or pointer_member)) {
        Marker_mut_p m = skip_space_forward(cursor + 1, end);
        if (m->token_type is T_IDENTIFIER) {
          m = skip_space_forward(m + 1, end);
          if (m->token_type is T_TUPLE_START) {
            ++m; // Point right after “(”.
            Marker_array_mut_slice o = (Marker_array_slice){
              .end_p = skip_space_back(start, cursor)
            };
            o.start_p = o.end_p;
            size_t nesting = 0;
            while (o.start_p is_not start) {
              switch ((o.start_p-1)->token_type) {
              case T_SPACE:
              case T_OP_1: case T_OP_2:
              case T_IDENTIFIER: case T_NUMBER:
                break;
              case T_INDEX_END:   ++nesting; break;
              case T_INDEX_START: --nesting; break;
              case T_TUPLE_END:
                if (not nesting and
                    o.start_p is_not o.end_p) goto found_expression_start;
                ++nesting;
                break;
              case T_TUPLE_START:
                --nesting;
                if (not nesting) goto found_expression_start;
                break;
              default:
                if (not nesting) goto found_expression_start;
              }
              --o.start_p;
            } found_expression_start:;
            if ((o.end_p-1)->token_type is T_TUPLE_END) --o.end_p;
            o.start_p = skip_space_forward(o.start_p, o.end_p);
            if (value_member) push_Marker_array(&replacement, addressOf);
            push_Marker_array(&replacement, void_cast);
            append_Marker_array(&replacement, o);
            if (skip_space_forward(m, end)->token_type is_not T_TUPLE_END) {
              push_Marker_array(&replacement, comma);
              push_Marker_array(&replacement, space);
            }
            // Invalidates: markers
            cursor_position = (size_t)(m - start);
            splice_Marker_array(markers,
                                cursor_position,
                                0, NULL,
                                bounds_of_Marker_array(&replacement));
            start = start_of_Marker_array(markers);
            end   =   end_of_Marker_array(markers);
            cursor = start + cursor_position + replacement.len;
            destruct_Marker_array(&replacement);
          }
        }
      }
    }
    ++cursor;
  }

  if (err.message) error_at(err.message, err.position, markers, src);
}
