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

#include "arena.h"

#include <stdlib.h>
#include <string.h>

;
;
;

// --- Types ---
typedef struct ArenaNode {
    char* mem;
    usize cap;
    usize used;
    struct ArenaNode* next;
} ArenaNode;

void zmm_arena_init(ArenaAlloc* arena) { arena->head = NULL; }

void* zmm_arena_alloc(ArenaAlloc* arena, usize size) {
    if (!arena->head || arena->head->used + size > arena->head->cap) {
        usize cap = size > 65536 ? size : 65536;
        ArenaNode* node = malloc(sizeof(ArenaNode));
        node->mem = malloc(cap);
        node->cap = cap;
        node->used = 0;
        node->next = arena->head;
        arena->head = node;
    }
    void* ptr = arena->head->mem + arena->head->used;
    arena->head->used += size;
    return ptr;
}

// 8-byte aligned allocator (for structs, large integers, etc.)
void* zmm_arena_alloc_aligned(ArenaAlloc* arena, usize size) {
    if (arena->head) {
        // Pad the current `used` offset to the next 8-byte boundary
        usize padding = (8 - (arena->head->used & 7)) & 7;
        arena->head->used += padding;
    }
    // Defer to standard alloc. (If it triggers a new block, malloc
    // guarantees the base address of the new block is already aligned).
    return zmm_arena_alloc(arena, size);
}

void* zmm_arena_dupe(ArenaAlloc* arena, const void* mem, usize n) {
    void* arena_mem =
        zmm_arena_alloc(arena, n);  // Unaligned dupe is fine for strings
    memcpy(arena_mem, mem, n);
    return arena_mem;
}

void zmm_arena_free(ArenaAlloc* arena) {
    ArenaNode* curr = arena->head;
    while (curr) {
        ArenaNode* next = curr->next;
        free(curr->mem);
        free(curr);
        curr = next;
    }
    arena->head = NULL;
}
