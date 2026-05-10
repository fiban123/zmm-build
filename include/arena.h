#pragma once

#include "export.h"
#include "num.h"

struct ArenaNode;

typedef struct {
    struct ArenaNode* head;
} ArenaAlloc;

/**
 * Initializes the arena allocator.
 */
API void zmm_arena_init(ArenaAlloc* arena);

/**
 * Allocates a block of memory from the arena.
 * The memory is not guaranteed to be zero-initialized.
 */
API void* zmm_arena_alloc(ArenaAlloc* arena, usize size);

/**
 * Allocates an 8-byte aligned block of memory from the arena.
 * Useful for structs and other types that require alignment.
 */
API void* zmm_arena_alloc_aligned(ArenaAlloc* arena, usize size);

/**
 * Duplicates a memory block into the arena.
 */
API void* zmm_arena_dupe(ArenaAlloc* arena, const void* mem, usize n);

/**
 * Frees all memory associated with the arena and its nodes.
 */
API void zmm_arena_free(ArenaAlloc* arena);
