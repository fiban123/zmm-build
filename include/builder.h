#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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
    arr(struct Node) nodes;             // stb_ds dynamic array
    stringhm(struct NodeMap) node_map;  // stb_ds string hash map
} BuildGraph;

/**
 * Initialize the build graph.
 */
API void zmm_builder_init(BuildGraph* g);

/**
 * Free all memory associated with the graph.
 */
API void zmm_builder_free(BuildGraph* g);

/**
 * Add a target to the DAG.
 * @param sources Array of source files.
 * @param num_sources Number of source files.
 * @param output The output target file.
 * @param deps Optional array of extra dependencies.
 * @param num_deps Number of extra dependencies.
 */
API void zmm_builder_add(BuildGraph* g, SliceCU8 const* sources,
                         usize num_sources, SliceCU8 output,
                         SliceCU8 const* deps, usize num_deps);

/**
 * Execute the build for a SPECIFIC target, isolating its subgraph.
 * Runs tasks in parallel using a thread pool.
 * @return 0 on success, or non-zero if a build task failed or target wasn't
 * found.
 */
API int zmm_builder_build(BuildGraph* g, SliceCU8 target, BuilderFn builder);

/**
 * Returns true if B is newer than A, or A or B does not exist.
 * if it returns true, it likely means that A needs to be rebuilt.
 *
 * useful for just checking whether a single file needs to be rebuilt
 * without setting up a build graph.
 * */
API bool zmm_builder_is_dirty(SliceCU8 a_path, SliceCU8 b_path);
