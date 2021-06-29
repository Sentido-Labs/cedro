/* -*- coding: utf-8 c-basic-offset: 2 tab-width: 2 indent-tabs-mode: nil -*- */
/** \file */ /* Array template definition. */

/** `DESTRUCT_BLOCK` is a block of code that releases the resources for a
    block of objects of type T, between `mut_T_p cursor` and `T_p end`. \n
    For instance:                                                       \n
    `{ while (cursor != end) destruct_`T`(cursor++); }`                 \n
    If the type does not need any clean-up, just use `{}`.              \n
    Defines the types:                                                  \n
    `mut_`T`_array`, `mut_`T`_array_p`, `mut_`T`_array_mut_p`,          \n
    T`_array`, T`_array_p`, T`_array_mut_p`
*/
#define DEFINE_ARRAY_OF(T, PADDING, DESTRUCT_BLOCK)                     \
  /** The constant `PADDING_##T##_ARRAY` = `PADDING`                    \
      defines how many items are physically available                   \
      after those valid elements.                                    \n \
      This can be used to avoid special cases near the end              \
      when searching for fixed-length sequences in the array,           \
      although you have to set them to `0` or other appropriate value.  \
   */                                                                   \
typedef struct T##_array {                                              \
  /** Current length, the number of valid elements. */                  \
  size_t len;                                                           \
  /** Maximum length before reallocation is needed. */                  \
  size_t capacity;                                                      \
  /** The items stored in this array. */                                \
  T##_mut_p start;                                                      \
} mut_##T##_array, * const mut_##T##_array_p, * mut_##T##_array_mut_p;  \
typedef const struct T##_array                                          \
/*   */ T##_array, * const       T##_array_p, *       T##_array_mut_p;  \
                                                                        \
/**                                                                     \
   Example:                                                             \
   \code{.c}                                                            \
   T##_array_slice s = { &start, &start + 10 };                         \
   \endcode                                                             \
*/                                                                      \
typedef struct T##_array_slice {                                        \
  /** Start address. */                                                 \
  T##_mut_p start_p;                                                    \
  /** End addres. */                                                    \
  T##_mut_p end_p;                                                      \
} mut_##T##_array_slice, * const mut_##T##_array_slice_p,               \
  /**/                   *       mut_##T##_array_slice_mut_p;           \
typedef const struct T##_array_slice                                    \
/*   */ T##_array_slice, * const T##_array_slice_p,                     \
  /**/                   *       T##_array_slice_mut_p;                 \
                                                                        \
/**                                                                     \
   A slice of an array where the elements are <b>mut</b>able.           \
                                                                        \
   Example:                                                             \
   \code{.c}                                                            \
   T##_mut_array a;\n                                                   \
   init_##T##_array(&a, 0);\n                                           \
   T##_mut_array_slice s;\n                                             \
   init_##T##_array_slice(&s, &a, 3, 10);\n                             \
   assert(&s->start_p == &a->start + 3);\n                              \
   assert(&s->end_p   == &a->start + 10);                               \
   \endcode                                                             \
*/                                                                      \
typedef struct T##_array_mut_slice {                                    \
  /** Start address. */                                                 \
  mut_##T##_mut_p start_p;                                              \
  /** End addres. */                                                    \
  mut_##T##_mut_p end_p;                                                \
} /* Mutable slice of a mutable array whose items are read-only.        \
   */     mut_##T##_array_mut_slice,                                    \
  /* Pointer to mutable slice of a mutable array of read-only items. */ \
  * const mut_##T##_array_mut_slice_p,                                  \
  /* Mutable pointer to mutable slice of mutable read-only array.*/     \
  *       mut_##T##_array_mut_slice_mut_p;                              \
/**                                                                     \
   A slice of an array where the elements are <b>const</b>ants.         \
                                                                        \
   Example:                                                             \
   \code{.c}                                                            \
   T##_mut_array a;\n                                                   \
   init_##T##_array(&a, 0);\n                                           \
   T##_mut_array_slice s;\n                                             \
   init_##T##_array_slice(&s, &a, 3, 10);\n                             \
   assert(&s->start_p == &a->start + 3);\n                              \
   assert(&s->end_p   == &a->start + 10);                               \
   \endcode                                                             \
*/                                                                      \
typedef struct T##_array_mut_slice                                      \
/* Slice of a mutable array whose items are read-only.                  \
 */             T##_array_mut_slice,                                    \
  /* Pointer to mutable slice of a mutable array of read-only items. */ \
  * const       T##_array_mut_slice_p,                                  \
  /* Mutable pointer to mutable slice of mutable read-only array.*/     \
  *             T##_array_mut_slice_mut_p;                              \
                                                                        \
/** Initialize the slice to point at (*array_p)[`start`...`end`].       \
    `end` can be 0, in which case the slice extends to                  \
    the end of `array_p`. */                                            \
static void                                                             \
init_##T##_array_slice(mut_##T##_array_slice_p _,                       \
                       T##_array_p array_p,                             \
                       size_t start, size_t end)                        \
{                                                                       \
  _->start_p = array_p->start + start;                                  \
  if (end) _->end_p = array_p->start + end;                             \
  else     _->end_p = array_p->start + array_p->len;                    \
}                                                                       \
                                                                        \
/** Initialize the slice to point at (*array_p)[`start`...`end`].       \
    `end` can be 0, in which case the slice extends to                  \
    the end of `array_p`. */                                            \
static void                                                             \
init_##T##_array_mut_slice(mut_##T##_array_mut_slice_p _,               \
                           mut_##T##_array_mut_p array_p,               \
                           size_t start, size_t end)                    \
{                                                                       \
  _->start_p = (mut_##T##_mut_p) array_p->start + start;                \
  if (end) _->end_p = (mut_##T##_mut_p) array_p->start + end;           \
  else     _->end_p = (mut_##T##_mut_p) array_p->start + array_p->len;  \
}                                                                       \
                                                                        \
static void                                                             \
destruct_##T##_block(mut_##T##_mut_p cursor, T##_p end)                 \
{                                                                       \
  DESTRUCT_BLOCK                                                        \
}                                                                       \
                                                                        \
/** Initialize the array at the given pointer.                       \n \
    For local variables, use it like this:                           \n \
    \code{.c}                                                           \
    mut_##T##_array things;\n                                           \
    init_##T##_array(&things, 100); ///< We expect around 100 items.\n  \
    {...}\n                                                             \
    destruct_##T##_array(&things);\n                                    \
    \endcode                                                            \
 */                                                                     \
static void                                                             \
init_##T##_array(mut_##T##_array_p _, size_t initial_capacity)          \
{                                                                       \
  _->len = 0;                                                           \
  _->capacity = initial_capacity + PADDING;                             \
  _->start = _->capacity? malloc(_->capacity * sizeof(*_->start)): NULL;\
  /* Used malloc() here instead of calloc() because we need realloc()   \
     later anyway, so better keep the exact same behaviour. */          \
}                                                                       \
/** Initialize the array at the given pointer, as a view into a         \
    constant C array.                                                \n \
    Can be used for copy-on-write strings:                           \n \
    \code{.c}                                                           \
    mut_char_array text;\n                                              \
    init_from_constant_char_array(&text, "abce", 4);\n                  \
    push_char_array(&text, 'f');\n                                      \
    {...}\n                                                             \
    destruct_char_array(&text);\n                                       \
    \endcode                                                            \
 */                                                                     \
static void                                                             \
init_from_constant_##T##_array(mut_##T##_array_p _,                     \
                               T* items, size_t len)                    \
{                                                                       \
  _->len = len;                                                         \
  _->capacity = 0;                                                      \
  _->start = items;                                                     \
  /* Used malloc() here instead of calloc() because we need realloc()   \
     later anyway, so better keep the exact same behaviour. */          \
}                                                                       \
/** Heap-allocate and initialize a mut_##T##_array.                     \
 */                                                                     \
static mut_##T##_array                                                  \
new_##T##_array(size_t initial_capacity)                                \
{                                                                       \
  mut_##T##_array _;                                                    \
  init_##T##_array(&_, initial_capacity);                               \
  return _;                                                             \
}                                                                       \
/** Heap-allocate and initialize a mut_##T##_array.                     \
 */                                                                     \
static mut_##T##_array_p                                                \
new_##T##_array_p(size_t initial_capacity)                              \
{                                                                       \
  mut_##T##_array_p _ = malloc(sizeof(T##_array));                      \
  if (_) init_##T##_array(_, initial_capacity);                         \
  return _;                                                             \
}                                                                       \
/** Release any resources allocated for this struct.                 \n \
    Safe to call also for objects initialized as views over constants   \
    with `init_from_constant_##T##_array()`.                            \
 */                                                                     \
static void                                                             \
destruct_##T##_array(mut_##T##_array_p _)                               \
{                                                                       \
  if (_->capacity is_not 0) {                                           \
    /* _->capacity == 0 means that _->start is a non-owned pointer. */  \
    destruct_##T##_block((mut_##T##_p) _->start, _->start + _->len);    \
    free((mut_##T##_mut_p) (_->start));                                 \
    *((mut_##T##_mut_p *) &(_->start)) = NULL;                          \
    _->capacity = 0;                                                    \
  }                                                                     \
  _->len = 0;                                                           \
}                                                                       \
/** Delete this heap-allocated struct,                                  \
    and release any resources allocated for it.                      \n \
    Same as `destruct_##T##_array(_); free(_);`.                     \n \
    Safe to call also for objects initialized as views over constants   \
    with `init_from_constant_##T##_array()`.                            \
 */                                                                     \
static void                                                             \
free_##T##_array(mut_##T##_array_p _)                                   \
{                                                                       \
  destruct_##T##_array(_);                                              \
  free(_);                                                              \
}                                                                       \
/** Transfer ownership of any resources allocated for this struct.      \
    This just indicates that the caller is no longer responsible for    \
    releasing those resources.                                       \n \
   Example:                                                             \
   \code{.c}                                                            \
   T##_array a; init_##T##_array(&a, 10);\n                             \
   store_in_another_object(&obj, move_##T##_array(&a));\n               \
   \/\* No need to destruct a here. It is now obj’s problem. \*\/\n     \
   \/\/ However, it is still safe to call destruct_##T##_array():\n     \
   destruct_##T##_array(&a); \/\/ No effect since we transferred it.    \
   \endcode                                                             \
 */                                                                     \
static mut_##T##_array                                                  \
move_##T##_array(mut_##T##_array_p _)                                   \
{                                                                       \
  mut_##T##_array transferred_copy = *_;                                \
  _->len = 0;                                                           \
  _->capacity = 0;                                                      \
  *((mut_##T##_mut_p *) &(_->start)) = NULL;                            \
  return transferred_copy;                                              \
}                                                                       \
                                                                        \
/** Make sure that the array is ready to hold `minimum` elements,       \
    resizing the array if needed. */                                    \
static void                                                             \
ensure_capacity_##T##_array(mut_##T##_array_p _, size_t minimum)        \
{                                                                       \
  minimum += PADDING;                                                   \
  if (minimum <= _->capacity) return;                                   \
  if (_->capacity is 0) {                                               \
    /* _->capacity == 0 means that _->start is a non-owned pointer. */  \
    _->capacity = minimum + PADDING;                                    \
    _->start = malloc(_->capacity * sizeof(*_->start));                 \
  } else {                                                              \
    _->capacity = 2*_->capacity + PADDING;                              \
    if (minimum > _->capacity) _->capacity = minimum;                   \
    _->start = realloc((void*) _->start,                                \
                       _->capacity * sizeof(*_->start));                \
  }                                                                     \
}                                                                       \
                                                                        \
/** Push a bit copy of the element on the end/top of the array,         \
    resizing the array if needed. */                                    \
static void                                                             \
push_##T##_array(mut_##T##_array_p _, T item)                           \
{                                                                       \
  ensure_capacity_##T##_array(_, _->len + 1);                           \
  *((mut_##T##_p) _->start + _->len++) = item;                          \
}                                                                       \
                                                                        \
/** Splice the given `insert` slice in place of the removed elements,   \
    resizing the array if needed.                                       \
    Starting at `position`, `delete` elements are deleted and           \
    the elements in the `insert` slice are inserted there               \
    as bit copies.                                                      \
    If `deleted` is not `NULL`, the deleted elements are not destroyed  \
    but copied to that array.                                           \
    The `insert` slice must belong to a different array or be empty. */ \
static void                                                             \
splice_##T##_array(mut_##T##_array_p _,                                 \
                   size_t position, size_t delete,                      \
                   mut_##T##_array_p deleted,                           \
                   T##_array_slice insert)                              \
{                                                                       \
  assert(position + delete <= _->len);                                  \
  if (deleted) {                                                        \
    T##_array_slice slice = {                                           \
      .start_p = (mut_##T##_p) _->start + position,                     \
      .end_p   = (mut_##T##_p) _->start + position + delete             \
    };                                                                  \
    splice_##T##_array(deleted, deleted->len, 0, NULL, slice);          \
  } else {                                                              \
    destruct_##T##_block((mut_##T##_p) _->start + position,             \
                         _->start + position + delete);                 \
  }                                                                     \
                                                                        \
  size_t insert_len = 0;                                                \
  size_t new_len = _->len - delete;                                     \
  if (insert.start_p is_not insert.end_p) {                             \
    assert(_->start          > insert.end_p ||                          \
           _->start + _->len < insert.start_p);                         \
    assert(insert.end_p >= insert.start_p);                             \
    insert_len = (size_t)(insert.end_p - insert.start_p);               \
    new_len += insert_len;                                              \
    ensure_capacity_##T##_array(_, new_len);                            \
  }                                                                     \
                                                                        \
  size_t gap_end = position + insert_len;                               \
  memmove((void*) (_->start + gap_end),                                 \
          _->start + position + delete,                                 \
          (_->len - delete - position) * sizeof(*_->start));            \
  _->len = _->len + insert_len - delete;                                \
  if (insert_len) {                                                     \
    memcpy((void*) (_->start + position),                               \
           insert.start_p,                                              \
           insert_len * sizeof(*_->start));                             \
  }                                                                     \
}                                                                       \
                                                                        \
/** Append the given `insert` slice to the array.                       \
    Same as `splice_##T##_array(_, _->len, 0, NULL, insert)`.           \
    The `insert` slice, must belong to a different array. */            \
static void                                                             \
append_##T##_array(mut_##T##_array_p _, T##_array_slice insert)         \
{                                                                       \
  splice_##T##_array(_, _->len, 0, NULL, insert);                       \
}                                                                       \
                                                                        \
/** Delete `delete` elements from the array at `position`.              \
    Same as `splice_##T##_array(_, position, delete, NULL,              \
    (T##_array_slice){0})`. */                                          \
static void                                                             \
delete_##T##_array(mut_##T##_array_p _, size_t position, size_t delete) \
{                                                                       \
  assert(position + delete <= _->len);                                  \
  destruct_##T##_block((mut_##T##_p) _->start + position,               \
                   _->start + position + delete);                       \
                                                                        \
  memmove((void*) (_->start + position),                                \
          _->start + position + delete,                                 \
          (_->len - delete - position) * sizeof(*_->start));            \
  _->len -= delete;                                                     \
}                                                                       \
                                                                        \
/** Remove the element at the end/top of the array,                     \
    copying its bits into `*item_p` unless `item_p` is `0`,             \
    in which case destruct_##T##_block()                                \
    is called on those elements. */                                     \
static void                                                             \
pop_##T##_array(mut_##T##_array_p _, mut_##T##_p item_p)                \
{                                                                       \
  if (not _->len) return;                                               \
  mut_##T##_p last_p = (mut_##T##_p) _->start + _->len - 1;             \
  if (item_p) memmove((void*) last_p, item_p, sizeof(*_->start));       \
  else        destruct_##T##_block((mut_##T##_p) last_p, last_p + 1);   \
  --_->len;                                                             \
}                                                                       \
                                                                        \
/** Return a pointer to the element at `position`.                      \
    Panics if the index is out of range. */                             \
static T##_p                                                            \
get_##T##_array(T##_array_p _, size_t position)                         \
{                                                                       \
  assert(position < _->len);                                            \
  return _->start + position;                                           \
}                                                                       \
                                                                        \
/** Return a mutable pointer to the element at `position`.              \
    Panics if the index is out of range. */                             \
static mut_##T##_p                                                      \
get_mut_##T##_array(T##_array_p _, size_t position)                     \
{                                                                       \
  assert(position < _->len);                                            \
  return (mut_##T##_p) _->start + position;                             \
}                                                                       \
                                                                        \
/** Return a pointer to the start of the array (same as `_->start`) */  \
static T##_p                                                            \
start_of_##T##_array(T##_array_p _)                                     \
{                                                                       \
  return _->start;                                                      \
}                                                                       \
                                                                        \
/** Return a pointer to the next byte after                             \
    the element at the end of the array. */                             \
static T##_p                                                            \
end_of_##T##_array(T##_array_p _)                                       \
{                                                                       \
  return _->start + _->len;                                             \
}                                                                       \
                                                                        \
/** Return a pointer to the start of the array (same as `_->start`) */  \
static mut_##T##_p                                                      \
start_of_mut_##T##_array(mut_##T##_array_p _)                           \
{                                                                       \
  return (mut_##T##_p) _->start;                                        \
}                                                                       \
                                                                        \
/** Return a pointer to the next byte after                             \
    the element at the end of the array. */                             \
static mut_##T##_p                                                      \
end_of_mut_##T##_array(mut_##T##_array_p _)                             \
{                                                                       \
  return (mut_##T##_p) _->start + _->len;                               \
}                                                                       \
                                                                        \
/** Return the slice for the whole array. */                            \
static T##_array_slice                                                  \
bounds_of_##T##_array(T##_array_p _)                                    \
{                                                                       \
  return (T##_array_slice){ _->start, _->start + _->len };              \
}                                                                       \
                                                                        \
/** Return the slice for the whole array. */                            \
static mut_##T##_array_slice                                            \
bounds_of_mut_##T##_array(mut_##T##_array_p _)                          \
{                                                                       \
  return (mut_##T##_array_slice){ _->start, _->start + _->len };        \
}                                                                       \
                                                                        \
/** Return the index for the given pointer. */                          \
static size_t                                                           \
index_##T##_array(T##_array_p _, T##_p pointer)                         \
{                                                                       \
  return (size_t)(pointer - _->start);                                  \
}                                                                       \
static const size_t PADDING_##T##_ARRAY = PADDING//; commented out to avoid ;;.
