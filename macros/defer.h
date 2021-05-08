/// Simple `defer`-style functionality using the `auto` keyword.
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

/** Insert actions backwards up to the next level.
    Remove them from the list of pending actions. */
static size_t
insert_deferred_actions(DeferredAction_array_p pending,
                        size_t level,
                        Marker_p indentation, Marker_p indentation_between,
                        size_t cursor, mut_Marker_array_p markers)
{
  size_t inserted_length = 0;
  mut_Marker_array_slice indent = {
    .start_p = indentation,
    .end_p   = indentation + 1
  };
  DeferredAction_p     actions_start = DeferredAction_array_start(pending);
  DeferredAction_mut_p actions_cursor  = DeferredAction_array_end(pending);
  while (actions_cursor is_not actions_start) {
    --actions_cursor;
    if (actions_cursor->level <= level) break;
    mut_Marker_array_slice action = {
      .start_p = Marker_array_start(&actions_cursor->action),
      .end_p   = Marker_array_end  (&actions_cursor->action)
    };
    splice_Marker_array(markers, cursor + inserted_length, 0, NULL, &indent);
    if (inserted_length is 0) {
      indent.start_p = indentation_between;
      indent.end_p   = indentation_between + 1;
    }
    inserted_length += 1;
    splice_Marker_array(markers, cursor + inserted_length, 0, NULL, &action);
    inserted_length += actions_cursor->action.len;
  }
  return inserted_length;
}

static void
exit_level(mut_DeferredAction_array_p pending, size_t level)
{
  DeferredAction_p     actions_start = DeferredAction_array_start(pending);
  DeferredAction_mut_p actions_cursor  = DeferredAction_array_end(pending);
  while (actions_cursor is_not actions_start) {
    --actions_cursor;
    if (actions_cursor->level <= level) break;
    size_t cut_point = actions_cursor - actions_start;
    // Deleting items does not invalidate pointers in splice_...().
    splice_DeferredAction_array(pending, cut_point, pending->len - cut_point,
                                NULL, NULL);
  }
}

static void macro_defer(mut_Marker_array_p markers, mut_Byte_array_p src)
{
  defer_start();

  Marker space = new_marker(src, " ", T_SPACE);

  mut_Marker_array block_stack;
  init_Marker_array(&block_stack, 20);
  defer(&block_stack, &destruct_Marker_array);
  mut_DeferredAction_array pending;
  init_DeferredAction_array(&pending, 20);
  defer(&pending, &destruct_DeferredAction_array);

  mut_Marker_mut_p start  = (mut_Marker_p)Marker_array_start(markers);
  mut_Marker_mut_p cursor = start;
  mut_Marker_mut_p end    = (mut_Marker_p)Marker_array_end(markers);
  size_t cursor_position;

  mut_Error err = { .position = 0, .message = NULL };
  mut_Marker_array marker_buffer;
  init_Marker_array(&marker_buffer, 8);
  defer(&marker_buffer, &destruct_Marker_array);

  while (cursor is_not end) {
    // TODO: should we try to handle forward goto?
    // TODO: should we try to handle initializers in for loops?
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
            log("At line %lu: %s",
                line_number(src, statement - start),
                "Too many opening parenthesis.");
            defer_end();
            return;
          }
          --nesting;
        } else if (not nesting and statement->token_type is_not T_SPACE) {
          if (statement->token_type is_not T_CONTROL_FLOW       and
              statement->token_type is_not T_CONTROL_FLOW_LOOP  and
              statement->token_type is_not T_CONTROL_FLOW_SWITCH) {
            statement = cursor;
          }
          break;
        }
      }
      push_Marker_array(&block_stack, *statement);
      ++cursor;
    } else if (cursor->token_type is T_BLOCK_END) {
      pop_Marker_array(&block_stack, NULL);
      Marker_mut_p previous_line = cursor;
      if (previous_line is_not start) {
        --previous_line;
        skip_space_back(start, previous_line);
        if (previous_line is_not start) --previous_line;
      }
      // if line starts with return, abort.
      if (previous_line is_not start) {
        previous_line = find_line_start(previous_line - 1, start, &err);
        if (err.message) {
          log("At line %lu: %s", line_number(src, err.position), err.message);
          err.message = NULL;
          break;
        }
        skip_space_forward(previous_line, end);
        if (previous_line->token_type is T_CONTROL_FLOW_RETURN) {
          ++cursor;
          continue;
        }
      }
      size_t block_level = block_stack.len;
      if (previous_line is_not start) {
        Marker_p insertion_point =
            (cursor-1)->token_type is T_SPACE? cursor-1: cursor;
        Marker before =
            (previous_line-1)->token_type is T_SPACE? *(previous_line-1): space;
        Marker between = indentation(src, previous_line->start);
        // Invalidates: markers
        cursor_position = cursor - start
            + insert_deferred_actions(&pending, block_level,
                                      &before, &between,
                                      insertion_point - start, markers);
        start = (mut_Marker_p)Marker_array_start(markers);
        end   = (mut_Marker_p)Marker_array_end  (markers);
        cursor = start + cursor_position;
      }
      exit_level(&pending, block_level);
      ++cursor;
    } else if (cursor->token_type is T_CONTROL_FLOW_BREAK) {
      size_t block_level = block_stack.len;
      if (block_level is 0) {
        log("At line %lu: break outside of block.",
            line_number(src, cursor->start));
        err.message = NULL;
        break;
      }
      while (block_level) {
        --block_level;
        TokenType block_type =
            get_Marker_array(&block_stack, block_level)->token_type;
        if (block_type is T_CONTROL_FLOW_LOOP or
            block_type is T_CONTROL_FLOW_SWITCH) {
          break;
        }
      }
      if (cursor is_not start) {
        Marker_p insertion_point =
            (cursor-1)->token_type is T_SPACE? cursor-1: cursor;
        Marker before = insertion_point is_not cursor? *insertion_point: space;
        Marker between = indentation(src, cursor->start);
        // Invalidates: markers
        cursor_position = cursor - start
            + insert_deferred_actions(&pending, block_level,
                                      &before, &between,
                                      insertion_point - start, markers);
        start = (mut_Marker_p)Marker_array_start(markers);
        end   = (mut_Marker_p)Marker_array_end  (markers);
        cursor = start + cursor_position;
      }
      ++cursor;
    } else if (cursor->token_type is T_CONTROL_FLOW_RETURN) {
      // Same as for T_CONTROL_FLOW_BREAK, only with block_level = 0:
      size_t block_level = 0;
      if (cursor is_not start) {
        Marker_p insertion_point =
            (cursor-1)->token_type is T_SPACE? cursor-1: cursor;
        Marker before = insertion_point is_not cursor? *insertion_point: space;
        Marker between = indentation(src, cursor->start);
        // Invalidates: markers
        cursor_position = cursor - start
            + insert_deferred_actions(&pending, block_level,
                                      &before, &between,
                                      insertion_point - start, markers);
        start = (mut_Marker_p)Marker_array_start(markers);
        end   = (mut_Marker_p)Marker_array_end  (markers);
        cursor = start + cursor_position;
      }
      ++cursor;
    } else if (cursor->token_type is T_TYPE_QUALIFIER_AUTO) {
      defer_enter();
      // Add a new action at the current level.
      // First skip the auto keyword and whitespace:
      mut_Marker_mut_p action_start = cursor + 1;
      skip_space_forward(action_start, end);
      // Now find the end of the statement:
      Marker_mut_p action_end = action_start;
      if (action_end->token_type is T_CONTROL_FLOW       or
          action_end->token_type is T_CONTROL_FLOW_LOOP  or
          action_end->token_type is T_CONTROL_FLOW_SWITCH) {
        ++action_end;
        skip_space_forward(action_end, end);
        size_t nesting = 0;
        while (action_end is_not end) {
          if (T_TUPLE_START == action_end->token_type) {
            ++nesting;
          } else if (T_TUPLE_END == action_end->token_type) {
            if (not nesting) {
              log("At line %lu: %s",
                  line_number(src, action_end - start),
                  "Too many closing parenthesis.");
              defer_end();
              return;
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
        log("At line %lu: %s", line_number(src, err.position), err.message);
        err.message = NULL;
        break;
      }

      if (action_end is action_start) {
        log("At line %lu: %s",
            line_number(src, err.position),
            "Empty auto statement.");
        break;
      }

      mut_Marker_p line_start = (mut_Marker_p)
          find_line_start(cursor, start, &err);
      if (err.message) {
        log("At line %lu: %s", line_number(src, err.position), err.message);
        err.message = NULL;
        break;
      }
      marker_buffer.len = 0;
      // Invalidates: markers
      cursor_position = cursor - start;
      splice_Marker_array(markers,
                          line_start - start,
                          action_end - line_start,
                          &marker_buffer, NULL);
      start = (mut_Marker_p)Marker_array_start(markers);
      end   = (mut_Marker_p)Marker_array_end  (markers);
      cursor = start + cursor_position;
      // Delete indentation and auto keyword:
      splice_Marker_array(&marker_buffer,
                          0, action_start - line_start,
                          NULL, NULL);
      // Copy buffer into pending mut_DeferredAction_array:
      mut_DeferredAction deferred = {
        .level = block_stack.len,
        .action = move_Marker_array(&marker_buffer)
      };
      push_DeferredAction_array(&pending, move_DeferredAction(&deferred));

      defer_exit();
      // Do not increase cursor because we just removed the tokens here,
      // so the next one will be at the same location.
    } else {
      ++cursor;
    }
  }

  defer_end();
}
