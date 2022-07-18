/* -*- coding: utf-8 c-basic-offset: 2 tab-width: 2 indent-tabs-mode: nil -*-
 * vi: set et ts=2 sw=2: */
/// Simple `defer`-style functionality using the `auto` keyword.
static void
macro_defer(mut_Marker_array_p markers, mut_Byte_array_p src);

typedef struct DeferredAction {
  size_t level;
  mut_Marker_array action;
} MUT_CONST_TYPE_VARIANTS(DeferredAction);

/* Stack of pending deferred actions. */
DEFINE_ARRAY_OF(DeferredAction, 0, {
    while (cursor is_not end) {
      destruct_Marker_array(&(cursor++->action));
    }
  });

static void
destruct_DeferredAction(mut_DeferredAction_p _)
{
  destruct_Marker_array(&_->action);
}

static DeferredAction
move_DeferredAction(mut_DeferredAction_p _)
{
  DeferredAction copy = *_;
  move_Marker_array(&_->action);
  return copy;
}

static bool
are_there_pending_deferred_actions(DeferredAction_array_p pending, size_t level)
{
  if (pending->len is 0) return false;
  DeferredAction_p last = end_of_DeferredAction_array(pending) - 1;
  return (last->level >= level);
}

static Marker newline_marker = {0};
static bool
is_newline_marker(Marker m)
{
  return m.len is 0 and m.token_type is T_NONE;
}

/** Insert actions backwards up to and including the given level.
    Returns the number of tokens inserted. */
static size_t
insert_deferred_actions(DeferredAction_array_p pending, size_t level,
                        Marker_array_slice line,
                        Marker indentation, Marker extra_indentation,
                        size_t cursor, mut_Marker_array_p markers)
{
  mut_Marker between[2] = { indentation, extra_indentation };

  size_t inserted_length = 0;
  Marker_array_slice indent = {
    &between[0],
    &between[between[1].len? 2: 1]
  };
  DeferredAction_p     actions_start  = start_of_DeferredAction_array(pending);
  DeferredAction_mut_p actions_cursor = end_of_DeferredAction_array(pending);
  while (actions_cursor is_not actions_start) {
    --actions_cursor;
    if (actions_cursor->level < level) break;

    if (inserted_length is_not 0) {
      splice_Marker_array(markers, cursor + inserted_length, 0, NULL, indent);
      inserted_length += len_Marker_array_slice(indent);
    }

    Marker_array_mut_slice action =
        bounds_of_Marker_array(&actions_cursor->action);

    mut_Marker_array action_indented =
        init_Marker_array(actions_cursor->action.len + 10);
    for (Marker_mut_p p = action.start_p; p is_not action.end_p; ++p) {
      if (is_newline_marker(*p)) {
        append_Marker_array(&action_indented, indent);
      } else {
        push_Marker_array(&action_indented, *p);
      }
    }
    action = bounds_of_Marker_array(&action_indented);
    splice_Marker_array(markers, cursor + inserted_length, 0, NULL, action);
    destruct_Marker_array(&action_indented);

    inserted_length += len_Marker_array_slice(action);
  }
  if (line.start_p) {
    splice_Marker_array(markers, cursor + inserted_length, 0, NULL, indent);
    inserted_length += len_Marker_array_slice(indent);
    splice_Marker_array(markers, cursor + inserted_length, 0, NULL, line);
    inserted_length += len_Marker_array_slice(line);
  }
  return inserted_length;
}

/** Eliminate pending deferred actions whose level is greater or equal
    than the given level.
 */
static void
exit_level(mut_DeferredAction_array_p pending, size_t level)
{
  DeferredAction_p     actions_start = start_of_DeferredAction_array(pending);
  DeferredAction_mut_p actions_cursor =  end_of_DeferredAction_array(pending);
  while (actions_cursor is_not actions_start) {
    --actions_cursor;
    if (actions_cursor->level < level) {
      ++actions_cursor;
      break;
    }
  }
  size_t cut_point = index_DeferredAction_array(pending, actions_cursor);
  // Deleting items at the end does not invalidate pointers in splice_...().
  splice_DeferredAction_array(pending, cut_point, pending->len - cut_point,
                              NULL, (DeferredAction_array_slice){0});
}

static void
macro_defer(mut_Marker_array_p markers, mut_Byte_array_p src)
{
  Marker space       = Marker_from(src, " ", T_SPACE);
  Marker block_start = Marker_from(src, "{", T_BLOCK_START);
  Marker block_end   = Marker_from(src, "}", T_BLOCK_END);
  Marker break_goto    = Marker_from(src, "goto", T_CONTROL_FLOW_BREAK);
  Marker continue_goto = Marker_from(src, "goto", T_CONTROL_FLOW_CONTINUE);
  mut_Marker indent_one_level = { .start = 0, .len = 0, .token_type = T_NONE };

  mut_TokenType_array block_stack  = init_TokenType_array(20);
  mut_DeferredAction_array pending = init_DeferredAction_array(20);

  mut_Marker_mut_p start  = start_of_mut_Marker_array(markers);
  mut_Marker_mut_p cursor = start;
  mut_Marker_mut_p end    = end_of_mut_Marker_array(markers);
  size_t cursor_position;

  mut_Error err = { .position = NULL, .message = NULL };
  mut_Marker_array marker_buffer = init_Marker_array(8);

  while (cursor is_not end) {
    if (cursor->token_type is T_BLOCK_START) {
      // Now find the start of the statement:
      Marker_mut_p statement = cursor;
      size_t nesting = 0;
      while (statement is_not start) {
        --statement;
        if (T_TUPLE_END is statement->token_type) {
          ++nesting;
        } else if (T_TUPLE_START is statement->token_type) {
          if (not nesting) {
            // C99 struct initializer: (type name){ ... }
            break;
          }
          --nesting;
        } else if (not nesting and
                   statement->token_type is_not T_SPACE and
                   statement->token_type is_not T_COMMENT) {
          if (statement->token_type is_not T_IDENTIFIER        and
              statement->token_type is_not T_CONTROL_FLOW_IF   and
              statement->token_type is_not T_CONTROL_FLOW_LOOP and
              statement->token_type is_not T_CONTROL_FLOW_SWITCH) {
            statement = cursor;
          }
          break;
        }
      }
      push_TokenType_array(&block_stack, statement->token_type);
      ++cursor;

      if (cursor is_not end and cursor->token_type is T_SPACE and
          indent_one_level.token_type is T_NONE) {
        Byte_array_slice slice = slice_for_marker(src, cursor);
        for (Byte_mut_p cursor = slice.end_p;
             cursor is_not slice.start_p;
             --cursor) {
          if (*(cursor - 1) is '\n') {
            init_Marker(&indent_one_level, cursor, slice.end_p, src, T_SPACE);
            indent_one_level.synthetic = true;
            break;
          }
        }
      }
    } else if (cursor->token_type is T_BLOCK_END) {
      size_t block_level = block_stack.len;
      if (not are_there_pending_deferred_actions(&pending, block_level)) {
        pop_TokenType_array(&block_stack, NULL);
        ++cursor;
        continue;
      }

      Marker_mut_p previous_line = cursor;
      if (previous_line is_not start) --previous_line;
      previous_line = skip_space_back(start, previous_line);
      if (previous_line is_not start) --previous_line;

      // If previous line diverts control flow, abort.
      // The deferred actions will have been already inserted before it.
      previous_line = find_line_start(previous_line, start, &err);
      if (err.message) {
        error_at(err.message, err.position, markers, src);
        err.message = NULL;
        goto free_all_and_return;
      }
      previous_line = skip_space_forward(previous_line, end);
      if (previous_line->token_type is T_CONTROL_FLOW_BREAK    or
          previous_line->token_type is T_CONTROL_FLOW_CONTINUE or
          previous_line->token_type is T_CONTROL_FLOW_GOTO     or
          previous_line->token_type is T_CONTROL_FLOW_RETURN) {
        goto exit_level_and_continue;
      }

      mut_Marker between = indentation(markers, cursor, false, src);
      between.synthetic = true;

      Marker_p insertion_point =
          cursor is_not start and (cursor-1)->token_type is T_SPACE?
          cursor-1: cursor;
      marker_buffer.len = 0;

      if (insertion_point->token_type is T_SPACE) {
        push_Marker_array(&marker_buffer, *insertion_point);
        push_Marker_array(&marker_buffer, indent_one_level);
      }
      insert_deferred_actions(&pending, block_level,
                              (Marker_array_slice){0},
                              between, indent_one_level,
                              marker_buffer.len, &marker_buffer);

      cursor_position = (size_t)(insertion_point - start);
      // Invalidates: markers
      splice_Marker_array(markers, cursor_position, 0, NULL,
                          bounds_of_Marker_array(&marker_buffer));
      cursor_position = cursor_position +
          marker_buffer.len /* Move to end of inserted block. */ +
          1                 /* Move to next marker.           */;

      start  = start_of_mut_Marker_array(markers);
      end    =   end_of_mut_Marker_array(markers);
      cursor = get_mut_Marker_array(markers, cursor_position);

   exit_level_and_continue:
      exit_level(&pending, block_stack.len);
      pop_TokenType_array(&block_stack, NULL);
      ++cursor;
    } else if (cursor->token_type is T_CONTROL_FLOW_BREAK    or
               cursor->token_type is T_CONTROL_FLOW_CONTINUE or
               cursor->token_type is T_CONTROL_FLOW_GOTO     or
               cursor->token_type is T_CONTROL_FLOW_RETURN) {
      size_t block_level = 0; // This is the correct value for ..._RETURN.
      Marker_mut_p label_p = skip_space_forward(cursor + 1, end);
      if (label_p is end or label_p->token_type is_not T_IDENTIFIER) {
        label_p = NULL;
      }
      if (cursor->token_type is T_CONTROL_FLOW_BREAK) {
        block_level = block_stack.len;
        if (block_level is 0) {
          error_at(LANG("break fuera de bloque.",
                        "break outside of block."),
                   cursor - 1, markers, src);
          err.message = NULL;
          break;
        }
        if (label_p) { // Label break.
          *cursor = break_goto; // Text “goto”, type T_CONTROL_FLOW_BREAK.
          goto handle_as_goto;
        }
        while (block_level) {
          --block_level;
          TokenType block_type =
              *get_TokenType_array(&block_stack, block_level);
          if (block_type is T_CONTROL_FLOW_LOOP or
              block_type is T_CONTROL_FLOW_SWITCH) {
            ++block_level;
            break;
          }
        }
      } else if (cursor->token_type is T_CONTROL_FLOW_CONTINUE) {
        block_level = block_stack.len;
        if (block_level is 0) {
          error_at(LANG("continue fuera de bloque.",
                        "continue outside of block."),
                   cursor - 1, markers, src);
          err.message = NULL;
          break;
        }
        if (label_p) { // Label continue.
          *cursor = continue_goto; // Text “goto”, type T_CONTROL_FLOW_CONTINUE.
          goto handle_as_goto;
        }
        while (block_level) {
          --block_level;
          TokenType block_type =
              *get_TokenType_array(&block_stack, block_level);
          if (block_type is T_CONTROL_FLOW_LOOP) {
            ++block_level;
            break;
          }
        }
      } else if (cursor->token_type is T_CONTROL_FLOW_GOTO) {
        block_level = block_stack.len;
        if (block_level is 0) {
          error_at(LANG("goto fuera de bloque.",
                        "goto outside of block."),
                   cursor - 1, markers, src);
          break;
        }
     handle_as_goto:;
        size_t function_level = block_level + 1;
        while (block_level) {
          --block_level;
          TokenType block_type =
              *get_TokenType_array(&block_stack, block_level);
          if (block_type is T_IDENTIFIER) {
            function_level = block_level + 1;
            break;
          }
        }

        if (not label_p) {
          error_at(LANG("goto sin etiqueta.",
                        "goto without label."),
                   cursor - 1, markers, src);
          break;
        }

        mut_Byte_array label = init_Byte_array(10);
        extract_src(label_p, label_p+1, src, &label);

        // Find minimum block level traversed to reach label.
        label_p = NULL;
        Marker_mut_p m;
        size_t nesting, low_watermark;
        block_level = block_stack.len;

        // First forward, because that’s the most usual case:
        m = cursor + 1;
        low_watermark = nesting = block_level;
        while (m is_not end and nesting >= function_level) {
          if        (m->token_type is T_BLOCK_START) {
            ++nesting;
          } else if (m->token_type is T_BLOCK_END) {
            --nesting;
            low_watermark = nesting;
          } else if (m->token_type is T_CONTROL_FLOW_LABEL) {
            if (src_eq(m, &label, src)) {
              label_p = m;
              break;
            }
          }
          ++m;
        }

        if (label_p) {
          if (low_watermark < nesting) {
            eprintln("Warning: forward goto jumps to another control flow branch.");
          }
        } else {
          // Then backwards:
          m = cursor - 1;
          low_watermark = nesting = block_level;
          while (m is_not start and nesting >= function_level) {
            if        (m->token_type is T_BLOCK_END) {
              ++nesting;
            } else if (m->token_type is T_BLOCK_START) {
              --nesting;
              low_watermark = nesting;
            } else if (m->token_type is T_CONTROL_FLOW_LABEL) {
              if (src_eq(m, &label, src)) {
                label_p = m;
                break;
              }
            }
            --m;
          }
        }

        if (label_p) {
          if (low_watermark < block_level) block_level = low_watermark;
          if (cursor->token_type is T_CONTROL_FLOW_BREAK) {
            if (label_p < cursor) {
              error_at(LANG("no se permiten saltos atrás con «break».",
                            "backward jumps are not allowed with “break”."),
                       skip_space_forward(cursor + 1, end) + 1, markers, src);
              label_p = NULL;
              break;
            }
            Marker_p before_label = label_p - 1 > cursor?
                skip_space_back(cursor, label_p - 1): label_p;
            if (low_watermark < nesting or
                before_label is_not start and
                (before_label-1)->token_type is_not T_BLOCK_END) {
              error_at(LANG("la etiqueta objetivo debe ir"
                            " justo después del bucle.",
                            "the target label must be"
                            " right after the loop."),
                       skip_space_forward(cursor + 1, end) + 1, markers, src);
              label_p = NULL;
              break;
            }
          } else if (cursor->token_type is T_CONTROL_FLOW_CONTINUE) {
            if (label_p > cursor) {
              error_at(LANG("no se permiten saltos adelante con «continue».",
                            "forward jumps are not allowed with “continue”."),
                       skip_space_forward(cursor + 1, end) + 1, markers, src);
              label_p = NULL;
              break;
            }
            Marker_p after_label = label_p + 2 < cursor?
                skip_space_forward(label_p + 2, cursor): label_p;
            if (low_watermark < nesting or
                after_label->token_type is_not T_CONTROL_FLOW_LOOP) {
              error_at(LANG("la etiqueta objetivo debe ir"
                            " justo antes del bucle.",
                            "the target label must be"
                            " right before the loop."),
                       skip_space_forward(cursor + 1, end) + 1, markers, src);
              label_p = NULL;
              break;
            }
          }
        }

        if (label_p) {
          ++block_level;
        } else {
          mut_Byte_array message = {0};
          push_fmt(&message,
                   LANG("no se encuentra la etiqueta «%s».",
                        "label “%s” not found."),
                   as_c_string(&label));
          error_at(as_c_string(&message),
                   skip_space_forward(cursor + 1, end), markers, src);
          destruct_Byte_array(&message);
          break;
        }

        destruct_Byte_array(&label);
      }

      if (not are_there_pending_deferred_actions(&pending, block_level)) {
        ++cursor;
        continue;
      }

      Marker_array_mut_slice line = { .start_p = NULL, .end_p = NULL };
      line.start_p = find_line_start(cursor, start, &err);
      if (err.message) {
        error_at(err.message, err.position, markers, src);
        err.message = NULL;
        break;
      }
      line.end_p = find_line_end(cursor, end, &err);
      if (err.message) {
        error_at(err.message, err.position, markers, src);
        err.message = NULL;
        break;
      }

      mut_Marker between = indentation(markers, line.start_p, true, src);
      between.synthetic = true;

      Marker_mut_p insertion_point = cursor;
      if (cursor is_not start and (cursor-1)->token_type is T_SPACE) {
        --insertion_point;
      }

      size_t delete_count;

      marker_buffer.len = 0;

      if (line.start_p is_not start and
          ((line.start_p+1)->token_type is T_CONTROL_FLOW_IF or
           (line.start_p+1)->token_type is T_CONTROL_FLOW_LOOP)
          ) {
        // We need to wrap this in a block.
        line.start_p = insertion_point;
        if (err.message) {
          error_at(err.message, err.position, markers, src);
          err.message = NULL;
          break;
        }
        if (line.end_p is_not end and line.end_p->token_type is T_SEMICOLON) {
          ++line.end_p;
        }
        line.start_p = skip_space_forward(line.start_p, line.end_p);
        insertion_point = line.start_p;

        // Invalidates: marker_buffer
        push_Marker_array(&marker_buffer, block_start);
        push_Marker_array(&marker_buffer, between);
        push_Marker_array(&marker_buffer, indent_one_level);
        insert_deferred_actions(&pending, block_level,
                                line,
                                between, indent_one_level,
                                marker_buffer.len, &marker_buffer);
        push_Marker_array(&marker_buffer, between);
        push_Marker_array(&marker_buffer, block_end);
        delete_count = len_Marker_array_slice(line);
      } else {
        // This is already a block, or we do not need it e.g. `case: …`.
        if (line.start_p->token_type is T_SPACE) {
          Byte_array_slice spaces = slice_for_marker(src, line.start_p);
          Byte_mut_p c = spaces.start_p;
          while (c < spaces.end_p and *c is_not '\n') ++c;
          if (c is spaces.end_p) {
            // If this statement is in the same line as the block start “{“,
            // use a single space instead of newline and indent.
            between = space;
          }
        }

        // Invalidates: marker_buffer
        if (insertion_point->token_type is T_SPACE) {
          push_Marker_array(&marker_buffer, *insertion_point);
        }
        insert_deferred_actions(&pending, block_level,
                                (Marker_array_slice){0},
                                between, (Marker){0},
                                marker_buffer.len, &marker_buffer);
        delete_count = 0;
      }

      // Here, line.start_p = insertion_point.
      cursor_position = (size_t)(insertion_point - start);
      // Invalidates: markers
      splice_Marker_array(markers, cursor_position, delete_count, NULL,
                          bounds_of_Marker_array(&marker_buffer));
      cursor_position = cursor_position +
          len_Marker_array_slice(line) +
          marker_buffer.len - delete_count; // Move to end of inserted block.

      start = start_of_mut_Marker_array(markers);
      end   =   end_of_mut_Marker_array(markers);
      cursor = start + cursor_position;
    } else if (cursor->token_type is T_CONTROL_FLOW_DEFER) {
      // Add a new action at the current level.
      // First skip the auto keyword and whitespace:
      Marker_mut_p action_start = skip_space_forward(cursor + 1, end);
      // Now find the end of the statement:
      Marker_mut_p action_end = action_start;
      if (action_end->token_type is T_CONTROL_FLOW_IF or
          action_end->token_type is T_CONTROL_FLOW_LOOP) {
        action_end = skip_space_forward(action_end + 1, end);
        size_t nesting = 0;
        while (action_end is_not end) {
          if (T_TUPLE_START is action_end->token_type) {
            ++nesting;
          } else if (T_TUPLE_END is action_end->token_type) {
            if (not nesting) {
              error_at(LANG("demasiados cierres de paréntesis.",
                            "too many closing parenthesis."),
                       action_end, markers, src);
              goto free_all_and_return;
            }
            --nesting;
            if (not nesting) {
              ++action_end;
              break;
            }
          }
          ++action_end;
        }
        action_end = skip_space_forward(action_end, end);
      }
      if (action_end is_not end and
          T_BLOCK_START is action_end->token_type) {
        // Find the end of the block.
        action_end = find_matching_fence(action_end, end, &err);
      } else {
        // This must be a semicolon-terminated line.
        action_end = find_line_end(action_end, end, &err);
        if (action_end is_not end) ++action_end;
      }
      if (err.message) {
        error_at(err.message, err.position, markers, src);
        err.message = NULL;
        break;
      }

      if (action_end is action_start) {
        error_at(LANG("sentencia auto vacía.",
                      "empty auto statement."),
                 action_end, markers, src);
        break;
      }

      Marker_p line_start = find_line_start(cursor, start, &err);
      if (err.message) {
        error_at(err.message, err.position, markers, src);
        err.message = NULL;
        break;
      }
      marker_buffer.len = 0;
      cursor_position = index_Marker_array(markers, line_start);
      // Cut deferred action from markers into marker_buffer.
      // Invalidates: markers, marker_buffer
      splice_Marker_array(markers,
                          (size_t)(line_start - start),
                          (size_t)(action_end - line_start), &marker_buffer,
                          (Marker_array_slice){0});
      start  = start_of_mut_Marker_array(markers);
      end    =   end_of_mut_Marker_array(markers);
      cursor = get_mut_Marker_array(markers, cursor_position);
      size_t indentation = 0;
      for (Marker_mut_p m = cursor; m is_not line_start; ) {
        --m;
        if (m->token_type is T_SPACE) {
          Byte_array_mut_slice s = slice_for_marker(src, m);
          Byte_mut_p c = s.end_p;
          while (c is_not s.start_p and (*--c is_not '\n')) ++indentation;
          if (c is_not s.start_p) break;
        }
      }
      // Delete indentation and auto keyword:
      splice_Marker_array(&marker_buffer,
                          0, (size_t)(action_start - line_start), NULL,
                          (Marker_array_slice){0});

      if (indentation is_not 0) {
        // If the deferred code spans multiple lines,
        // strip the initial indentation in each of them.
        for (mut_Marker_mut_p m = start_of_mut_Marker_array(&marker_buffer);
             m is_not end_of_mut_Marker_array(&marker_buffer); ++m) {
          if (m->token_type is T_SPACE) {
            Byte_array_mut_slice s = slice_for_marker(src, m);
            while (s.end_p is_not s.start_p) {
              if (*--s.end_p is '\n') { ++s.end_p; break; }
            }
            if (s.end_p is_not s.start_p) {
              if (len_Byte_array_slice(s) + indentation <= m->len) {
                m->len   -= len_Byte_array_slice(s) + indentation;
                m->start += len_Byte_array_slice(s) + indentation;
                cursor_position = index_Marker_array(&marker_buffer, m);
                // Invalidates: marker_buffer
                splice_Marker_array(&marker_buffer, cursor_position,
                                    0, NULL,
                                    (Marker_array_slice){
                                      &newline_marker,
                                      &newline_marker + 1
                                    });
                m = get_mut_Marker_array(&marker_buffer, cursor_position);
              }
            }
          }
        }
      }

      // Copy buffer into pending mut_DeferredAction_array:
      mut_DeferredAction deferred = {
        .level = block_stack.len,
        .action = move_Marker_array(&marker_buffer)
      };
      push_DeferredAction_array(&pending, move_DeferredAction(&deferred));

      // Do not increase cursor because we just removed the tokens here,
      // so the next one will be at the same location.
    } else {
      ++cursor;
    }
  }

free_all_and_return:
  destruct_Marker_array(&marker_buffer);
  destruct_DeferredAction_array(&pending);
  destruct_TokenType_array(&block_stack);
}
