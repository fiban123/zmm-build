/*
 * zmm-build: A compiled C build system
 * Copyright 2026 fiban123
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
