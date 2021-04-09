/** Resolve the types of expressions. W.I.P. */
static void
resolve_types(mut_Marker_array_p markers, Buffer_p src)
{
  mut_Marker_p m_start = (mut_Marker_p) Marker_array_start(markers);
  mut_Marker_p m_end   = (mut_Marker_p) Marker_array_end(markers);
  //char * const token = NULL;
  mut_Marker_mut_p previous = NULL;
  for (mut_Marker_mut_p m = m_start; m is_not m_end; ++m) {
    if (T_SPACE is m->token_type || T_COMMENT is m->token_type) continue;
    if (previous) {
      if ((T_TUPLE_START is m->token_type || T_OP_14 is m->token_type) &&
          T_IDENTIFIER is previous->token_type) {
        mut_Marker_mut_p p = previous;
        while (p is_not m_start) {
          --p;
          switch (p->token_type) {
            case T_TYPE_QUALIFIER:
            case T_TYPE_STRUCT:
            case T_TYPE:
            case T_SPACE:
            case T_COMMENT:
              continue;
            case T_IDENTIFIER:
              p->token_type = T_TYPE;
              break;
            case T_OP_3:
              p->token_type = T_OP_2;
              break;
            default:
              p = m_start;
          }
        }
      }
    }
    previous = m;
  }
}
