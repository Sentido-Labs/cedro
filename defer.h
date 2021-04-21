/* -*- coding: utf-8 c-basic-offset: 2 tab-width: 2 indent-tabs-mode: nil -*- */
/** \file Defer/autofree helper functions for C. */
/**
 * \mainpage
 *
 * A very simple, portable plain C implementation of `defer`-like functionality.
 * It can not track changes to the pointers, for instance:
 * ```
 * char* buffer = malloc(10);
 * defer(buffer, &free);
 * buffer = realloc(20);
 * defer_exit(); // FAILURE: the buffer pointer might have changed.
 * ```
 * It also requires explicit calls (macros) to register a clean-up function
 * for a variable and to call those pending functions when exiting a scope.
 *
 * \author Alberto González Palomo https://sentido-labs.com
 * \copyright ©2021 Alberto González Palomo https://sentido-labs.com
 *
 * Created: 2021-04-21 09:37
 */

TYPEDEF_STRUCT(DeferEntry, {
    size_t level;
    void* mem_p;
    void (*free)(void* mem_p);
  });
/** How many variables can be tracked per block.
    The maximum number of variables that can be tracked at once is
    `DEFER_BLOCK_SIZE` × `DEFER_MAX_BLOCKS`. */
#define DEFER_BLOCK_SIZE 16
/** Maximum number of blocks available:
    the first one is always allocated; the rest are allocated on demand. */
#define DEFER_MAX_BLOCKS 8
typedef mut_DeferEntry mut_DeferEntryBlock[DEFER_BLOCK_SIZE];
typedef mut_DeferEntryBlock *mut_DeferEntryBlock_p;
/** The first block is allocated in the stack,
    and subsequent ones in the heap. */
typedef struct DeferStack {
  size_t level;
  size_t len;
  mut_DeferEntry         first[DEFER_BLOCK_SIZE];
  mut_DeferEntryBlock_p blocks[DEFER_MAX_BLOCKS];
} mut_DeferStack, * mut_DeferStack_p;

/** Start a *defer* area where deferred deallocation functions can be added.
    This would normally be at the **beginning of a function**.
    @param[in] max maximum number of tracked variables. */
/* https://en.cppreference.com/w/c/language/array_initialization */
#define defer_start() mut_DeferStack defer_stack = \
  { 0, 0, {{0}}, {NULL} }; defer_stack.blocks[0] = &defer_stack.first

/** Enter a nested defer scope. It is not required, but this way you can
    free resources before the end of the function.
    This would normally be at the **beginning of a code block**. */
#define defer_enter() { ++defer_stack.level; }

/** Add a deferred deallocation entry to the list to be released when calling
    `defer_exit()` or `defer_end()`.
    @param[in] pointer object pointer that will be passed later to `function`.
    @param[in] destructor function that deallocates the resources
    held by object. If the object was itself allocated, you will have to
    call `free(pointer)` by yourself. */
#define defer(pointer, function) { size_t b = defer_stack.len / DEFER_BLOCK_SIZE; assert(b < DEFER_MAX_BLOCKS); size_t i = defer_stack.len % DEFER_BLOCK_SIZE; mut_DeferEntryBlock_p* bpp = &defer_stack.blocks[b]; if (NULL == *bpp) *bpp = malloc(DEFER_BLOCK_SIZE * sizeof(DeferEntry)); struct DeferEntry entry = { .level = defer_stack.level, .mem_p = pointer, .free = (void (*)(void*))function }; (**bpp)[i] = entry; ++defer_stack.len; }

/** Exit a nested defer scope.
    It should be paired with `defer_start()`, and it will call the
    destructor functions set since it.
    This will normally be at the **end of a code block**. */
#define defer_exit() { size_t b = defer_stack.len / DEFER_BLOCK_SIZE; size_t i = defer_stack.len % DEFER_BLOCK_SIZE; while (defer_stack.len) { if (i == 0) { i = DEFER_BLOCK_SIZE; --b; } --i; mut_DeferEntry_p entry = &(*defer_stack.blocks[b])[i]; if (entry->level < defer_stack.level) break; entry->free(entry->mem_p); --defer_stack.len; } }

/** End the defer area started by `defer_start()`, calling all pending
    destructors even those in nested scopes.
    In C, there can only be one defer area per function.
    This needs to be called at **each point where the function exits**. */
#define defer_end()  { size_t b = defer_stack.len / DEFER_BLOCK_SIZE; size_t i = defer_stack.len % DEFER_BLOCK_SIZE; while (defer_stack.len) { if (i == 0) { i = DEFER_BLOCK_SIZE; --b; } --i; mut_DeferEntry_p entry = &(*defer_stack.blocks[b])[i]; entry->free(entry->mem_p); --defer_stack.len; } }
