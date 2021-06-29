/// Simple `defer`-style functionality using the `auto` keyword.
static void
macro_defer(mut_Marker_array_p markers, mut_Byte_array_p src);

TYPEDEF_STRUCT(DeferredAction, {
    size_t level;
    mut_Marker_array action;
  });

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

/** Insert actions backwards up to and including the given level.
    Returns the number of tokens inserted. */
static size_t
insert_deferred_actions(DeferredAction_array_p pending, size_t level,
                        Marker_array_slice_p line,
                        Marker_p indentation, Marker_p extra_indentation,
                        size_t cursor, mut_Marker_array_p markers)
{
  mut_Marker between[2] = {
    *indentation,
    { .start = 0, .len = 0, .token_type = T_NONE }
  };
  if (extra_indentation) between[1] = *extra_indentation;

  size_t inserted_length = 0;
  mut_Marker_array_slice indent = {
    .start_p = &between[0],
    .end_p   = &between[between[1].len? 2: 1]
  };
  DeferredAction_p     actions_start  = start_of_DeferredAction_array(pending);
  DeferredAction_mut_p actions_cursor = end_of_DeferredAction_array(pending);
  while (actions_cursor is_not actions_start) {
    --actions_cursor;
    if (actions_cursor->level < level) break;

    if (inserted_length is_not 0) {
      splice_Marker_array(markers, cursor + inserted_length, 0, NULL, &indent);
      inserted_length += (size_t)(indent.end_p - indent.start_p);
    }

    mut_Marker_array_slice action =
        bounds_of_Marker_array(&actions_cursor->action);
    splice_Marker_array(markers, cursor + inserted_length, 0, NULL, &action);
    inserted_length += actions_cursor->action.len;
  }
  if (line) {
    splice_Marker_array(markers, cursor + inserted_length, 0, NULL, &indent);
    inserted_length += (size_t)(indent.end_p - indent.start_p);
    splice_Marker_array(markers, cursor + inserted_length, 0, NULL, line);
    inserted_length += (size_t)(line->end_p - line->start_p);
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
  // Deleting items does not invalidate pointers in splice_...().
  splice_DeferredAction_array(pending, cut_point, pending->len - cut_point,
                              NULL, NULL);
}

static void
macro_defer(mut_Marker_array_p markers, mut_Byte_array_p src)
{
  Marker space       = new_marker(src, " ", T_SPACE);
  Marker block_start = new_marker(src, "{", T_BLOCK_START);
  Marker block_end   = new_marker(src, "}", T_BLOCK_END);
  mut_Marker indent_one_level = { .start = 0, .len = 0, .token_type = T_NONE };

  mut_TokenType_array block_stack;
  init_TokenType_array(&block_stack, 20);
  mut_DeferredAction_array pending;
  init_DeferredAction_array(&pending, 20);

  mut_Marker_mut_p start  = start_of_mut_Marker_array(markers);
  mut_Marker_mut_p cursor = start;
  mut_Marker_mut_p end    = end_of_mut_Marker_array(markers);
  size_t cursor_position;

  mut_Error err = { .position = NULL, .message = NULL };
  mut_Marker_array marker_buffer;
  init_Marker_array(&marker_buffer, 8);

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
            eprintln("At line %lu: %s",
                     line_number(src, markers, statement),
                     "Too many opening parenthesis.");
            goto free_all_and_return;
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
      skip_space_back(start, previous_line);
      if (previous_line is_not start) --previous_line;

      // If previous line diverts control flow, abort.
      // The deferred actions will have been already inserted before it.
      previous_line = find_line_start(previous_line, start, &err);
      if (err.message) {
        eprintln("At line %lu: %s",
                 line_number(src, markers, err.position), err.message);
        err.message = NULL;
        goto free_all_and_return;
      }
      skip_space_forward(previous_line, end);
      if (previous_line->token_type is T_CONTROL_FLOW_BREAK    or
          previous_line->token_type is T_CONTROL_FLOW_CONTINUE or
          previous_line->token_type is T_CONTROL_FLOW_GOTO     or
          previous_line->token_type is T_CONTROL_FLOW_RETURN) {
        goto exit_level_and_continue;
      }

      Marker between = indentation(markers, cursor, false, src);

      Marker_p insertion_point =
          cursor is_not start and (cursor-1)->token_type is T_SPACE?
          cursor-1: cursor;
      marker_buffer.len = 0;

      if (insertion_point->token_type is T_SPACE) {
        push_Marker_array(&marker_buffer, *insertion_point);
        push_Marker_array(&marker_buffer, indent_one_level);
      }
      insert_deferred_actions(&pending, block_level,
                              NULL,
                              &between, &indent_one_level,
                              marker_buffer.len, &marker_buffer);

      mut_Marker_array_slice insert = bounds_of_Marker_array(&marker_buffer);
      cursor_position = (size_t)(insertion_point - start);
      // Invalidates: markers
      splice_Marker_array(markers,
                          cursor_position,
                          0, NULL,
                          &insert);
      cursor_position = cursor_position +
          1 +
          marker_buffer.len; // Move to end of inserted block.

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
      if (cursor->token_type is T_CONTROL_FLOW_BREAK) {
        block_level = block_stack.len;
        if (block_level is 0) {
          eprintln("At line %lu: break outside of block.",
                   line_number(src, markers, cursor));
          err.message = NULL;
          break;
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
          eprintln("At line %lu: break outside of block.",
                   line_number(src, markers, cursor));
          err.message = NULL;
          break;
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
          eprintln("At line %lu: goto outside of block.",
                   line_number(src, markers, cursor));
          break;
        }
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

        Marker_mut_p label_p = cursor + 1;
        skip_space_forward(label_p, end);
        if (label_p is end or label_p->token_type is_not T_IDENTIFIER) {
          eprintln("At line %lu: goto without label.",
                   line_number(src, markers, cursor));
          break;
        }
        mut_Byte_array label; init_Byte_array(&label, 10);
        extract_src(label_p, label_p+1, src, &label);

        // Find minimum block level traversed to reach label.
        label_p = NULL;
        Marker_mut_p m;
        size_t nesting;
        block_level = block_stack.len;

        // First forward, because that’s the most usual case:
        m = cursor + 1;
        nesting = block_level;
        while (m is_not end and nesting >= function_level) {
          if        (m->token_type is T_BLOCK_START) {
            ++nesting;
          } else if (m->token_type is T_BLOCK_END) {
            --nesting;
            if (nesting < block_level) block_level = nesting;
          } else if (m->token_type is T_CONTROL_FLOW_LABEL) {
            if (src_eq(m, &label, src)) {
              label_p = m;
              break;
            }
          }
          ++m;
        }

        if (!label_p) {
          // Then backwards:
          m = cursor - 1;
          nesting = block_level;
          while (m is_not start and nesting >= function_level) {
            if        (m->token_type is T_BLOCK_END) {
              ++nesting;
            } else if (m->token_type is T_BLOCK_START) {
              --nesting;
              if (nesting < block_level) block_level = nesting;
            } else if (m->token_type is T_CONTROL_FLOW_LABEL) {
              if (src_eq(m, &label, src)) {
                label_p = m;
                break;
              }
            }
            --m;
          }
        }

        if (label_p) ++block_level;
        else         block_level = block_stack.len;

        destruct_Byte_array(&label);
      }

      if (not are_there_pending_deferred_actions(&pending, block_level)) {
        ++cursor;
        continue;
      }

      mut_Marker_array_slice line = { .start_p = NULL, .end_p = NULL };
      line.start_p = find_line_start(cursor, start, &err);
      if (err.message) {
        eprintln("At line %lu: %s",
                 line_number(src, markers, err.position), err.message);
        err.message = NULL;
        break;
      }
      line.end_p = find_line_end(cursor, end, &err);
      if (err.message) {
        eprintln("At line %lu: %s",
                 line_number(src, markers, err.position), err.message);
        err.message = NULL;
        break;
      }

      mut_Marker between = indentation(markers, line.start_p, true, src);

      Marker_mut_p insertion_point = cursor;
      if (cursor is_not start and (cursor-1)->token_type is T_SPACE) {
        --insertion_point;
      }

      size_t delete_count;

      marker_buffer.len = 0;

      if (line.start_p is_not start and
          ((line.start_p+1)->token_type is T_CONTROL_FLOW_IF ||
           (line.start_p+1)->token_type is T_CONTROL_FLOW_LOOP)
          ) {
        // We need to wrap this in a block.
        line.start_p = insertion_point;
        if (err.message) {
          eprintln("At line %lu: %s",
                   line_number(src, markers, err.position), err.message);
          err.message = NULL;
          break;
        }
        if (line.end_p is_not end and line.end_p->token_type is T_SEMICOLON) {
          ++line.end_p;
        }
        skip_space_forward(line.start_p, line.end_p);
        insertion_point = line.start_p;

        // Invalidates: marker_buffer
        push_Marker_array(&marker_buffer, block_start);
        push_Marker_array(&marker_buffer, between);
        push_Marker_array(&marker_buffer, indent_one_level);
        insert_deferred_actions(&pending, block_level,
                                &line,
                                &between, &indent_one_level,
                                marker_buffer.len, &marker_buffer);
        push_Marker_array(&marker_buffer, between);
        push_Marker_array(&marker_buffer, block_end);
        delete_count = (size_t)(line.end_p - line.start_p);
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
                                NULL,
                                &between, NULL,
                                marker_buffer.len, &marker_buffer);
        delete_count = 0;
      }

      mut_Marker_array_slice insert = bounds_of_Marker_array(&marker_buffer);
      // Here, line.start_p = insertion_point.
      cursor_position = (size_t)(insertion_point - start);
      // Invalidates: markers
      splice_Marker_array(markers,
                          cursor_position,
                          delete_count, NULL,
                          &insert);
      cursor_position = cursor_position +
          (size_t)(line.end_p - line.start_p) +
          marker_buffer.len - delete_count; // Move to end of inserted block.

      start = start_of_mut_Marker_array(markers);
      end   =   end_of_mut_Marker_array(markers);
      cursor = start + cursor_position;
    } else if (cursor->token_type is T_TYPE_QUALIFIER_AUTO) {
      // Add a new action at the current level.
      // First skip the auto keyword and whitespace:
      mut_Marker_mut_p action_start = cursor + 1;
      skip_space_forward(action_start, end);
      // Now find the end of the statement:
      Marker_mut_p action_end = action_start;
      if (action_end->token_type is T_CONTROL_FLOW_IF or
          action_end->token_type is T_CONTROL_FLOW_LOOP) {
        ++action_end;
        skip_space_forward(action_end, end);
        size_t nesting = 0;
        while (action_end is_not end) {
          if (T_TUPLE_START == action_end->token_type) {
            ++nesting;
          } else if (T_TUPLE_END == action_end->token_type) {
            if (not nesting) {
              eprintln("At line %lu: %s",
                       line_number(src, markers, action_end),
                       "Too many closing parenthesis.");
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
        skip_space_forward(action_end, end);
        if (action_end is_not end and
            T_BLOCK_START is action_end->token_type) {
          // Find the end of the block.
          action_end = find_matching_fence(action_end, end, &err);
        } else {
          // This must be a semicolon-terminated line.
          action_end = find_line_end(action_end, end, &err);
          if (action_end is_not end) ++action_end;
        }
      } else {
        // This must be a semicolon-terminated line.
        action_end = find_line_end(action_end, end, &err);
        if (action_end is_not end) ++action_end;
      }
      if (err.message) {
        eprintln("At line %lu: %s",
                 line_number(src, markers, err.position), err.message);
        err.message = NULL;
        break;
      }

      if (action_end is action_start) {
        eprintln("At line %lu: %s",
                 line_number(src, markers, err.position),
                 "Empty auto statement.");
        break;
      }

      mut_Marker_p line_start = (mut_Marker_p)
          find_line_start(cursor, start, &err);
      if (err.message) {
        eprintln("At line %lu: %s",
                 line_number(src, markers, err.position), err.message);
        err.message = NULL;
        break;
      }
      marker_buffer.len = 0;
      cursor_position = index_Marker_array(markers, cursor);
      // Invalidates: markers
      splice_Marker_array(markers,
                          (size_t)(line_start - start),
                          (size_t)(action_end - line_start),
                          &marker_buffer, NULL);
      start  = start_of_mut_Marker_array(markers);
      end    =   end_of_mut_Marker_array(markers);
      cursor = get_mut_Marker_array(markers, cursor_position);
      // Delete indentation and auto keyword:
      splice_Marker_array(&marker_buffer,
                          0, (size_t)(action_start - line_start),
                          NULL, NULL);
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
