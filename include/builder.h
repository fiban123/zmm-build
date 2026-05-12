#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "arena.h"
#include "arr.h"
#include "export.h"
#include "slice.h"

// The builder function.
// Returns 0 on success. A non-zero return value will cancel the entire build.
typedef int (*BuilderFn)(const SliceCU8* sources, usize num_sources,
                         SliceCU8 output, u32 thread_idx);

struct Node;
struct NodeMap;

typedef struct {
    arr(struct Node) nodes;  // stb_ds dynamic array
    void* node_map;          // stb_ds string hash map
    ArenaAlloc* arena;
} BuildGraph;

/**
 * Initialize the build graph.
 */
API void zmm_bg_init(BuildGraph* g, ArenaAlloc* arena);

/**
 * Free all memory associated with the graph.
 */
API void zmm_bg_free(BuildGraph* g);

/**
 * Add a target to the DAG. all inputs are copied
 * into the arena to ensure they persist during the build.
 * @param sources Array of source files.
 * @param num_sources Number of source files.
 * @param output The output target file.
 * @param deps Optional array of extra dependencies.
 * @param num_deps Number of extra dependencies.
 */
API void zmm_bg_add(BuildGraph* g, SliceCU8 const* sources, usize num_sources,
                    SliceCU8 output, SliceCU8 const* deps, usize num_deps);

/**
 * Execute the build for a SPECIFIC target, isolating its subgraph.
 * Runs tasks in parallel using a thread pool.
 * @return 0 on success, or non-zero if a build task failed or target wasn't
 * found.
 */
API int zmm_bg_build(BuildGraph* g, SliceCU8 target, BuilderFn builder);

/**
 * Returns true if B is newer than A, or A or B does not exist.
 * if it returns true, it likely means that A needs to be rebuilt.
 *
 * useful for just checking whether a single file needs to be rebuilt
 * without setting up a build graph.
 * */
API bool zmm_bg_is_dirty(SliceCU8 a_path, SliceCU8 b_path);

// A lightweight handle to a target being constructed.
typedef struct {
    BuildGraph* g;
    u64 id;
} TargetBuilder;

// --- Target Builder API ---
API void zmm_tg_init_out(TargetBuilder* tg, BuildGraph* bg, SliceCU8 output);

// 2. Add Sources
API void zmm_tg_add_src(TargetBuilder* tb, SliceCU8 const* sources,
                        usize count);
API void zmm_tg_add_src_nc(TargetBuilder* tb, SliceCU8 const* sources,
                           usize count);  // Fast/No-copy

// 3. Add Dependencies
API void zmm_tg_add_dep(TargetBuilder* tb, SliceCU8 const* deps, usize count);
API void zmm_tg_add_dep_nc(TargetBuilder* tb, SliceCU8 const* deps,
                           usize count);  // Fast/No-copy
