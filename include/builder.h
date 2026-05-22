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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "arena.h"
#include "arr.h"
#include "export.h"
#include "slice.h"

/**
 * The builder callback that executes a build task.
 *
 * @param sources Array of source file paths.
 * @param num_sources Number of source files.
 * @param output Path to the output file.
 * @param thread_idx Index of the thread executing the task.
 * @return 0 on success, non-zero to abort the build.
 *
 * [Note] Typically calls a compiler or linker via zmm_sys_exec_print.
 */
typedef int (*BuilderFn)(const SliceCU8* sources, usize num_sources,
                         SliceCU8 output, u32 thread_idx);

struct Node;
struct NodeMap;

/**
 * A build graph (DAG) representing targets and their dependencies. Uses
 * an arena for nearly all internal allocations.
 */
typedef struct {
    arr(struct Node) nodes;  // stb_ds dynamic array
    void* node_map;          // khash string hash map
    ArenaAlloc* arena;
    u64 num_builds;
} BuildGraph;

/**
 * Initializes the build graph.
 *
 * @param g Pointer to the graph to initialize.
 * @param arena Pointer to an arena for internal allocations.
 */
API void zmm_bg_init(BuildGraph* g, ArenaAlloc* arena);

/**
 * Frees memory associated with the buildgraph. Does not free the arena.
 *
 * @param g Pointer to the graph to free.
 */
API void zmm_bg_free(BuildGraph* g);

/**
 * Adds a target to the DAG.
 *
 * @param g Pointer to the graph.
 * @param sources Array of source files.
 * @param num_sources Number of source files.
 * @param output The output target file.
 * @param deps Optional array of extra dependencies.
 * @param num_deps Number of extra dependencies.
 *
 * [Note] All inputs are copied into the arena to ensure they persist. To avoid
 * this, see [TargetBuilder]
 */
API void zmm_bg_add(BuildGraph* g, SliceCU8 const* sources, usize num_sources,
                    SliceCU8 output, SliceCU8 const* deps, usize num_deps);

/**
 * Executes the build for an array of targets, isolating its subgraph. It calls
 * the builder function for any target which needs to be built.
 *
 * @param g Pointer to the graph.
 * @param target The target to build.
 * @param builder The function used to build each target.
 * @return 0 on success, -1 if a task failed or target wasn't found.
 *
 * [Note] Runs tasks in parallel using a thread pool.
 */
API int zmm_bg_build(BuildGraph* g, const SliceCU8* targets, usize num_targets,
                     BuilderFn builder);

/**
 * Returns true if the `a` needs to be rebuilt, based on `b`.
 *
 * @param a_path The output file path.
 * @param b_path The input file path.
 * @return true if b_path is newer than a_path, or either does not exist.
 *
 * [Note] Useful for simple checks without a full build graph.
 */
API bool zmm_bg_is_dirty(SliceCU8 a_path, SliceCU8 b_path);

/**
 * A lightweight handle to a target being constructed. Allows for
 * much more complex behaviour than a simple zmm_bg_add call. There
 * are also non-copying versions.
 */
typedef struct {
    BuildGraph* g;
    u64 id;
} TargetBuilder;

/**
 * Initializes a target builder for a specific output.
 *
 * @param tg Pointer to the target builder.
 * @param bg Pointer to the build graph.
 * @param output The output target file.
 */
API void zmm_tg_init_out(TargetBuilder* tg, BuildGraph* bg, SliceCU8 output);

/**
 * Adds sources to the target being built.
 *
 * @param tb Pointer to the target builder.
 * @param sources Array of source files.
 * @param count Number of source files.
 */
API void zmm_tg_add_src(TargetBuilder* tb, SliceCU8 const* sources,
                        usize count);

/**
 * Adds sources to the target without copying them.
 *
 * @param tb Pointer to the target builder.
 * @param sources Array of source files.
 * @param count Number of source files.
 *
 * [Note] Sources must persist in memory.
 */
API void zmm_tg_add_src_nc(TargetBuilder* tb, SliceCU8 const* sources,
                           usize count);

/**
 * Adds extra dependencies to the target.
 *
 * @param tb Pointer to the target builder.
 * @param deps Array of dependency files.
 * @param count Number of dependency files.
 */
API void zmm_tg_add_dep(TargetBuilder* tb, SliceCU8 const* deps, usize count);

/**
 * Adds extra dependencies to the target without copying them.
 *
 * @param tb Pointer to the target builder.
 * @param deps Array of dependency files.
 * @param count Number of dependency files.
 *
 * [Note] Deps must persist in memory.
 */
API void zmm_tg_add_dep_nc(TargetBuilder* tb, SliceCU8 const* deps,
                           usize count);
