/*
 * zmm-build: A compiled C build system
 * Copyright (C) 2026 @fiban123
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "export.h"
#include "num.h"

struct ArenaNode;

/**
 * An arena allocator for efficient memory management.
 * Memory is allocated in large blocks (nodes) and handed out in smaller
 * pieces. All memory in the arena is freed at once.
 */
typedef struct {
    struct ArenaNode* head;
} ArenaAlloc;

/**
 * Initializes the arena allocator.
 * 
 * @param arena Pointer to the arena to initialize.
 */
API void zmm_arena_init(ArenaAlloc* arena);

/**
 * Allocates a block of memory from the arena.
 * 
 * @param arena Pointer to the arena.
 * @param size Number of bytes to allocate.
 * @return Pointer to the allocated memory.
 * 
 * [Note] The memory is not guaranteed to be zero-initialized.
 */
API void* zmm_arena_alloc(ArenaAlloc* arena, usize size);

/**
 * Allocates an 8-byte aligned block of memory from the arena.
 * 
 * @param arena Pointer to the arena.
 * @param size Number of bytes to allocate.
 * @return Pointer to the allocated memory.
 * 
 * [Note] Useful for ints or other types that require alignment.
 */
API void* zmm_arena_alloc_aligned(ArenaAlloc* arena, usize size);

/**
 * Duplicates a memory block into the arena.
 * 
 * @param arena Pointer to the arena.
 * @param mem Pointer to the memory to duplicate.
 * @param n Number of bytes to duplicate.
 * @return Pointer to the new memory block in the arena.
 */
API void* zmm_arena_dupe(ArenaAlloc* arena, const void* mem, usize n);

/**
 * Frees all memory associated with the arena and its nodes.
 * 
 * @param arena Pointer to the arena to free.
 */
API void zmm_arena_free(ArenaAlloc* arena);
