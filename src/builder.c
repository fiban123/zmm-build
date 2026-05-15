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

#include "builder.h"

#include <khash/khash.h>
#include <pthread.h>
#include <stb/stb_ds.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "arena.h"
#include "arr.h"
#include "cpu.h"
#include "slice.h"

typedef u64 NodeId;

// --- khash Map Implementation ---

// FNV-1 hash implementation for length-based string slices
static inline khint_t hash_slicecu8(SliceCU8 s) {
    khint_t h = 2166136261U;
    for (usize i = 0; i < s.len; ++i) {
        h *= 16777619U;
        h ^= s.ptr[i];
    }
    return h;
}

// Equality check for length-based string slices
static inline int eq_slicecu8(SliceCU8 a, SliceCU8 b) {
    if (a.len != b.len) return 0;
    return memcmp(a.ptr, b.ptr, a.len) == 0;
}

// Instantiate khash map: Key=SliceCU8, Value=NodeId
KHASH_INIT(node_map, SliceCU8, NodeId, 1, hash_slicecu8, eq_slicecu8)

typedef struct Node {
    SliceCU8 output;

    // stb_ds dynamic arrays
    arr(SliceCU8) sources;
    arr(SliceCU8) deps;

    // Edges
    NodeId* dependents;  // Nodes that wait for us
    NodeId* waits_on;  // Nodes we wait for (used for target subgraph traversal)

    bool is_target;

    // Execution state
    bool is_in_subgraph;
    _Atomic u32 pending_deps;
} Node;

// --- Task Queue Implementation ---

typedef struct {
    NodeId* buffer;
    int capacity;
    int head;
    int tail;
    int count;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    bool shutdown;
} TaskQueue;

static void queue_init(TaskQueue* q, int capacity) {
    q->buffer = (NodeId*)malloc(capacity * sizeof(NodeId));
    q->capacity = capacity;
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    q->shutdown = false;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->not_empty, NULL);
}

static void queue_free(TaskQueue* q) {
    free(q->buffer);
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->not_empty);
}

static void queue_push(TaskQueue* q, NodeId node) {
    pthread_mutex_lock(&q->lock);
    q->buffer[q->tail] = node;
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
}

static bool queue_pop(TaskQueue* q, NodeId* out_node) {
    pthread_mutex_lock(&q->lock);
    while (q->count == 0 && !q->shutdown) {
        pthread_cond_wait(&q->not_empty, &q->lock);
    }
    if (q->shutdown && q->count == 0) {
        pthread_mutex_unlock(&q->lock);
        return false;
    }
    *out_node = q->buffer[q->head];
    q->head = (q->head + 1) % q->capacity;
    q->count--;
    pthread_mutex_unlock(&q->lock);
    return true;
}

static void queue_shutdown(TaskQueue* q) {
    pthread_mutex_lock(&q->lock);
    q->shutdown = true;
    pthread_cond_broadcast(&q->not_empty);  // Wake up all threads to exit
    pthread_mutex_unlock(&q->lock);
}

// --- String Allocation Utilities ---

#define MAX_TEMP_PATH 4096
static _Thread_local char tl_path_buf[MAX_TEMP_PATH];

static inline char* slice_to_temp_cstr(SliceCU8 slice, bool* out_is_heap) {
    *out_is_heap = false;
    char* cstr = tl_path_buf;
    if (slice.len < MAX_TEMP_PATH) {
        memcpy(cstr, slice.ptr, slice.len);
        cstr[slice.len] = '\0';
    } else {
        *out_is_heap = true;
        cstr = (char*)malloc(slice.len + 1);
        memcpy(cstr, slice.ptr, slice.len);
        cstr[slice.len] = '\0';
    }
    return cstr;
}

// Helper to conveniently stat a slice without leaking memory.
static inline int stat_slice(SliceCU8 path, struct stat* out_stat) {
    bool is_heap;
    char* cstr = slice_to_temp_cstr(path, &is_heap);
    int res = stat(cstr, out_stat);
    if (is_heap) free(cstr);
    return res;
}

// --- Graph Utilities ---

void zmm_bg_init(BuildGraph* g, ArenaAlloc* arena) {
    g->nodes = arrinit;
    g->node_map = kh_init(node_map);
    g->arena = arena;
}

void zmm_bg_free(BuildGraph* g) {
    if (!g) return;

    for (usize i = 0; i < arrlenu(g->nodes); ++i) {
        arrfree(g->nodes[i].sources);
        arrfree(g->nodes[i].deps);
        arrfree(g->nodes[i].dependents);
        arrfree(g->nodes[i].waits_on);
    }
    arrfree(g->nodes);

    if (g->node_map) {
        kh_destroy(node_map, (khash_t(node_map)*)g->node_map);
    }
}

static NodeId get_or_put_node(BuildGraph* g, SliceCU8 path) {
    khash_t(node_map)* h = (khash_t(node_map)*)g->node_map;

    khint_t k = kh_get(node_map, h, path);
    if (k != kh_end(h)) {
        return kh_val(h, k);
    }

    // New node, so we need persistent memory for the map key.
    // Allocate it in the arena. No null terminator needed for khash.
    SliceCU8 perm_key;
    perm_key.len = path.len;
    perm_key.ptr = (const u8*)zmm_arena_dupe(g->arena, path.ptr, path.len);

    NodeId id = (NodeId)arrlenu(g->nodes);
    Node n = {0};

    // Reuse the persistent arena slice for the node output
    n.output = perm_key;
    arrpush(g->nodes, n);

    // Put in map
    int ret;
    k = kh_put(node_map, h, perm_key, &ret);
    kh_val(h, k) = id;

    return id;
}

void zmm_bg_add(BuildGraph* g, SliceCU8 const* sources, usize num_sources,
                SliceCU8 output, SliceCU8 const* deps, usize num_deps) {
    NodeId out_id = get_or_put_node(g, output);
    g->nodes[out_id].is_target = true;

    // Duplicate memory for sources into arena
    for (usize i = 0; i < num_sources; ++i) {
        SliceCU8 duped;
        duped.len = sources[i].len;
        duped.ptr =
            (const u8*)zmm_arena_dupe(g->arena, sources[i].ptr, sources[i].len);
        arrpush(g->nodes[out_id].sources, duped);
    }

    // Duplicate memory for deps into arena
    for (usize i = 0; i < num_deps; ++i) {
        SliceCU8 duped;
        duped.len = deps[i].len;
        duped.ptr =
            (const u8*)zmm_arena_dupe(g->arena, deps[i].ptr, deps[i].len);
        arrpush(g->nodes[out_id].deps, duped);
    }

    SliceCU8 const* lists[2] = {sources, deps};
    usize counts[2] = {num_sources, num_deps};

    for (int list_idx = 0; list_idx < 2; ++list_idx) {
        for (usize i = 0; i < counts[list_idx]; ++i) {
            NodeId dep_id = get_or_put_node(g, lists[list_idx][i]);
            arrpush(g->nodes[dep_id].dependents, out_id);
            arrpush(g->nodes[out_id].waits_on, dep_id);
        }
    }
}

// --- Concurrent Execution ---

typedef struct {
    BuildGraph* graph;
    BuilderFn builder;
    TaskQueue* queue;
    _Atomic bool* has_error;
    _Atomic u32* remaining_nodes;
    pthread_mutex_t* done_lock;
    pthread_cond_t* done_cond;
    u32 thread_idx;
} WorkerCtx;

// returns true if b is newer than a
static inline bool is_dirty_stat(const struct stat* a_stat,
                                 const struct stat* b_stat) {
    long a_sec = a_stat->st_mtime;
    long b_sec = b_stat->st_mtime;

#ifdef _WIN32
    // Windows/MinGW standard stat lacks nanosecond precision
    long a_nsec = 0;
    long b_nsec = 0;
#elif defined(__APPLE__)
    // macOS and BSD-based systems
    long a_nsec = a_stat->st_mtimespec.tv_nsec;
    long b_nsec = b_stat->st_mtimespec.tv_nsec;
#else
    // Linux and standard POSIX systems
    long a_nsec = a_stat->st_mtim.tv_nsec;
    long b_nsec = b_stat->st_mtim.tv_nsec;
#endif

    if (b_sec > a_sec) return true;
    if (a_sec == b_sec && b_nsec > a_nsec) return true;

    return false;
}

static bool is_dirty(SliceCU8 output, SliceCU8 const* inputs, usize num_inputs,
                     SliceCU8* deps, usize n_deps) {
    struct stat out_stat;
    if (stat_slice(output, &out_stat) != 0)
        return true;  // Output doesn't exist

    for (usize i = 0; i < num_inputs; ++i) {
        struct stat in_stat;
        if (stat_slice(inputs[i], &in_stat) != 0) return true;
        if (is_dirty_stat(&out_stat, &in_stat)) return true;
    }

    for (usize i = 0; i < n_deps; ++i) {
        struct stat in_stat;
        if (stat_slice(deps[i], &in_stat) != 0) return true;
        if (is_dirty_stat(&out_stat, &in_stat)) return true;
    }

    return false;
}

// returns true if b is newer than a (a needs to be rebuilt)
bool zmm_bg_is_dirty(SliceCU8 a_path, SliceCU8 b_path) {
    struct stat a_stat, b_stat;

    if (stat_slice(a_path, &a_stat) != 0) return true;
    if (stat_slice(b_path, &b_stat) != 0) return true;

    return is_dirty_stat(&a_stat, &b_stat);
}

static void* worker_fn(void* arg) {
    WorkerCtx* ctx = (WorkerCtx*)arg;
    NodeId node_id;

    while (queue_pop(ctx->queue, &node_id)) {
        // If an error occurred elsewhere, drain the queue without building
        // to cleanly exit and unblock the main thread.
        if (atomic_load_explicit(ctx->has_error, memory_order_acquire)) {
            u32 rem = atomic_fetch_sub(ctx->remaining_nodes, 1);
            if (rem == 1) {
                pthread_mutex_lock(ctx->done_lock);
                pthread_cond_signal(ctx->done_cond);
                pthread_mutex_unlock(ctx->done_lock);
            }
            continue;
        }

        Node* node = &ctx->graph->nodes[node_id];

        if (node->is_target) {
            bool dirty =
                is_dirty(node->output, node->sources, arrlenu(node->sources),
                         node->deps, arrlenu(node->deps));

            if (dirty) {
                int res = ctx->builder(node->sources, arrlenu(node->sources),
                                       node->output, ctx->thread_idx);
                if (res != 0) {
                    // Set error flag; subsequent queue pops will fast-fail
                    atomic_store_explicit(ctx->has_error, true,
                                          memory_order_release);
                }
            }
        }

        // Notify dependents
        for (usize i = 0; i < arrlenu(node->dependents); ++i) {
            NodeId dep_id = node->dependents[i];
            Node* dep_node = &ctx->graph->nodes[dep_id];

            // Only care about dependents that are in our target subgraph
            if (dep_node->is_in_subgraph) {
                u32 prev = atomic_fetch_sub(&dep_node->pending_deps, 1);
                if (prev == 1) {  // We were the last dependency
                    queue_push(ctx->queue, dep_id);
                }
            }
        }

        // Signal completion of this node
        u32 rem = atomic_fetch_sub(ctx->remaining_nodes, 1);
        if (rem == 1) {
            pthread_mutex_lock(ctx->done_lock);
            pthread_cond_signal(ctx->done_cond);
            pthread_mutex_unlock(ctx->done_lock);
        }
    }
    return NULL;
}

// Recursively walks down dependencies to mark the subgraph we actually care
// about
static void mark_subgraph(BuildGraph* g, NodeId start_id,
                          _Atomic u32* out_remaining) {
    // Use an stb_ds dynamic array as a manual stack
    NodeId* stack = NULL;
    arrpush(stack, start_id);

    while (arrlen(stack) > 0) {
        // Pop the last node added (LIFO behavior for DFS)
        NodeId id = arrpop(stack);
        Node* n = &g->nodes[id];

        // If we've already visited this node through another dependency path,
        // skip it
        if (n->is_in_subgraph) {
            continue;
        }

        // 1. Mark as part of the current build subgraph
        n->is_in_subgraph = true;

        // 2. Initialize the atomic counter for the scheduler
        atomic_store(&n->pending_deps, (u32)arrlenu(n->waits_on));

        // 3. Track total nodes to know when the build is finished
        atomic_fetch_add(out_remaining, 1);

        // 4. Add all dependencies to the stack to be processed
        for (usize i = 0; i < arrlenu(n->waits_on); ++i) {
            NodeId dep_id = n->waits_on[i];

            // Optimization: Only push if the dependent isn't already marked
            if (!g->nodes[dep_id].is_in_subgraph) {
                arrpush(stack, dep_id);
            }
        }
    }

    // Clean up the manual stack memory
    arrfree(stack);
}

int zmm_bg_build(BuildGraph* g, const SliceCU8* targets, usize num_targets,
                 BuilderFn builder) {
    khash_t(node_map)* h = (khash_t(node_map)*)g->node_map;

    // Reset subgraph state for a clean run
    for (usize i = 0; i < arrlenu(g->nodes); ++i) {
        g->nodes[i].is_in_subgraph = false;
    }

    _Atomic u32 remaining_nodes;
    atomic_init(&remaining_nodes, 0);

    // 1. Resolve all target nodes and mark their combined subgraphs
    for (usize i = 0; i < num_targets; ++i) {
        khint_t target_idx = kh_get(node_map, h, targets[i]);
        if (target_idx == kh_end(h)) {
            return 1;  // Target not found in the graph
        }

        NodeId target_id = kh_val(h, target_idx);

        // mark_subgraph safely ignores nodes already marked by a previous
        // target, so overlapping dependencies are naturally unioned and only
        // counted once.
        mark_subgraph(g, target_id, &remaining_nodes);
    }

    u32 total_nodes = atomic_load(&remaining_nodes);
    if (total_nodes == 0) return 0;  // Nothing to do

    // Size the queue to the max possible nodes so push() never has to block
    TaskQueue queue;
    queue_init(&queue, total_nodes);

    _Atomic bool has_error;
    atomic_init(&has_error, false);

    pthread_mutex_t done_lock;
    pthread_cond_t done_cond;
    pthread_mutex_init(&done_lock, NULL);
    pthread_cond_init(&done_cond, NULL);

    u32 num_threads = zmm_cpu_thread_count();
    pthread_t* threads = (pthread_t*)malloc(num_threads * sizeof(pthread_t));
    WorkerCtx* ctxs = (WorkerCtx*)malloc(num_threads * sizeof(WorkerCtx));

    // Spin up thread pool
    for (u32 i = 0; i < num_threads; ++i) {
        ctxs[i] = (WorkerCtx){.graph = g,
                              .builder = builder,
                              .queue = &queue,
                              .has_error = &has_error,
                              .remaining_nodes = &remaining_nodes,
                              .done_lock = &done_lock,
                              .done_cond = &done_cond,
                              .thread_idx = i};
        pthread_create(&threads[i], NULL, worker_fn, &ctxs[i]);
    }

    // Seed the queue with leaf nodes (nodes with 0 pending dependencies)
    for (usize i = 0; i < arrlenu(g->nodes); ++i) {
        if (g->nodes[i].is_in_subgraph &&
            atomic_load(&g->nodes[i].pending_deps) == 0) {
            queue_push(&queue, (NodeId)i);
        }
    }

    // Main thread blocks here until the target subgraph is fully resolved
    pthread_mutex_lock(&done_lock);
    while (atomic_load(&remaining_nodes) > 0) {
        pthread_cond_wait(&done_cond, &done_lock);
    }
    pthread_mutex_unlock(&done_lock);

    // Tear down thread pool
    queue_shutdown(&queue);
    for (u32 i = 0; i < num_threads; ++i) {
        pthread_join(threads[i], NULL);
    }

    free(threads);
    free(ctxs);
    pthread_mutex_destroy(&done_lock);
    pthread_cond_destroy(&done_cond);
    queue_free(&queue);

    return atomic_load(&has_error) ? -1 : 0;
}

static void add_dependency_edge(BuildGraph* g, NodeId target_id,
                                SliceCU8 dep_path) {
    // Note: get_or_put_node might resize g->nodes. Because we use NodeId
    // (indices) instead of pointers, this is 100% safe.
    NodeId dep_id = get_or_put_node(g, dep_path);
    arrpush(g->nodes[dep_id].dependents, target_id);
    arrpush(g->nodes[target_id].waits_on, dep_id);
}

void zmm_tg_init_out(TargetBuilder* tg, BuildGraph* g, SliceCU8 output) {
    NodeId id = get_or_put_node(g, output);
    g->nodes[id].is_target = true;
    tg->g = g;
    tg->id = id;
}

void zmm_tg_add_src(TargetBuilder* tb, SliceCU8 const* sources, usize count) {
    for (usize i = 0; i < count; ++i) {
        SliceCU8 duped;
        duped.len = sources[i].len;
        duped.ptr = (const u8*)zmm_arena_dupe(tb->g->arena, sources[i].ptr,
                                              sources[i].len);

        arrpush(tb->g->nodes[tb->id].sources, duped);
        add_dependency_edge(tb->g, tb->id, sources[i]);
    }
}

void zmm_tg_add_src_nc(TargetBuilder* tb, SliceCU8 const* sources,
                       usize count) {
    for (usize i = 0; i < count; ++i) {
        // Push the un-copied slice directly
        arrpush(tb->g->nodes[tb->id].sources, sources[i]);
        add_dependency_edge(tb->g, tb->id, sources[i]);
    }
}

void zmm_tg_add_dep(TargetBuilder* tb, SliceCU8 const* deps, usize count) {
    for (usize i = 0; i < count; ++i) {
        SliceCU8 duped;
        duped.len = deps[i].len;
        duped.ptr =
            (const u8*)zmm_arena_dupe(tb->g->arena, deps[i].ptr, deps[i].len);

        arrpush(tb->g->nodes[tb->id].deps, duped);
        add_dependency_edge(tb->g, tb->id, deps[i]);
    }
}

void zmm_tg_add_dep_nc(TargetBuilder* tb, SliceCU8 const* deps, usize count) {
    for (usize i = 0; i < count; ++i) {
        // Push the un-copied slice directly
        arrpush(tb->g->nodes[tb->id].deps, deps[i]);
        add_dependency_edge(tb->g, tb->id, deps[i]);
    }
}
