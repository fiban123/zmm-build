/*
 * zmm-build: A compiled C build system
 * Copyright 2026 fiban123
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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

#include "arr.h"
#include "cpu.h"
#include "str.h"

// --- Constants & Types ---

#define NODE_FLAG_PHONY (1 << 0)
#define NODE_FLAG_ALWAYS_DIRTY (1 << 1)
#define NODE_FLAG_IS_TARGET (1 << 2)

typedef u64 NodeId;

// --- khash Map Implementation ---

// FNV-1 hash implementation for StringView
static inline khint_t hash_strview(StringView s) {
    khint_t h = 2166136261U;
    for (usize i = 0; i < s.len; ++i) {
        h *= 16777619U;
        h ^= (u8)s.ptr[i];
    }
    return h;
}

static inline int eq_strview(StringView a, StringView b) {
    return zmm_str_eq(a, b);
}

KHASH_INIT(node_map, StringView, NodeId, 1, hash_strview, eq_strview)

// --- Graph Data Structures ---

typedef struct Node {
    StringView output;

    arr(StringView) sources;
    arr(StringView) deps;

    arr(NodeId) dependents;
    arr(NodeId) waits_on;

    BuilderFn builder;
    u8 flags;  // Merged boolean states

    // Planner / Execution state
    bool is_in_subgraph;
    bool eval_visited;
    bool is_dirty;
    _Atomic u32 pending_deps;
} Node;

struct BuildGraph {
    Node* nodes;
    void* node_map;          // khash_t(node_map)*
    char** internal_allocs;  // Replaces the arena: tracks duplicated strings

    _Atomic u32 remaining_nodes;
};

struct TargetBuilder {
    BuildGraph* g;
    NodeId id;
};

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
    pthread_cond_broadcast(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
}

// --- Internal Memory & String Utilities ---

// Hidden "Arena" mechanism that guarantees all strings are copied
static StringView bg_dupe_str(BuildGraph* g, StringView v) {
    char* ptr = (char*)malloc(v.len + 1);
    memcpy(ptr, v.ptr, v.len);
    ptr[v.len] = '\0';
    arrpush(g->internal_allocs, ptr);  // Track for cleanup
    return (StringView){ptr, v.len};
}

#define MAX_TEMP_PATH 4096
static _Thread_local char tl_path_buf[MAX_TEMP_PATH];

static inline char* strview_to_temp_cstr(StringView view, bool* out_is_heap) {
    *out_is_heap = false;
    char* cstr = tl_path_buf;
    if (view.len < MAX_TEMP_PATH) {
        memcpy(cstr, view.ptr, view.len);
        cstr[view.len] = '\0';
    } else {
        *out_is_heap = true;
        cstr = (char*)malloc(view.len + 1);
        memcpy(cstr, view.ptr, view.len);
        cstr[view.len] = '\0';
    }
    return cstr;
}

static inline int stat_strview(StringView path, struct stat* out_stat) {
    bool is_heap;
    char* cstr = strview_to_temp_cstr(path, &is_heap);
    int res = stat(cstr, out_stat);
    if (is_heap) free(cstr);
    return res;
}

// --- Graph Initialization & Teardown ---

void zmm_bg_init(BuildGraph* g) {
    g->nodes = arrinit;
    g->node_map = kh_init(node_map);
    g->internal_allocs = arrinit;
    atomic_init(&g->remaining_nodes, 0);
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

    for (usize i = 0; i < arrlenu(g->internal_allocs); ++i) {
        free(g->internal_allocs[i]);
    }
    arrfree(g->internal_allocs);
}

static NodeId get_or_put_node(BuildGraph* g, StringView path) {
    khash_t(node_map)* h = (khash_t(node_map)*)g->node_map;

    khint_t k = kh_get(node_map, h, path);
    if (k != kh_end(h)) {
        return kh_val(h, k);
    }

    StringView perm_key = bg_dupe_str(g, path);

    NodeId id = (NodeId)arrlenu(g->nodes);
    Node n = {0};
    n.output = perm_key;
    arrpush(g->nodes, n);

    int ret;
    k = kh_put(node_map, h, perm_key, &ret);
    kh_val(h, k) = id;

    return id;
}

static void add_dependency_edge(BuildGraph* g, NodeId target_id,
                                StringView dep_path) {
    NodeId dep_id = get_or_put_node(g, dep_path);
    arrpush(g->nodes[dep_id].dependents, target_id);
    arrpush(g->nodes[target_id].waits_on, dep_id);
}

// --- Public Graph Addition API ---

void zmm_bg_add(BuildGraph* g, const StringView* sources, usize num_sources,
                StringView output, const StringView* deps, usize num_deps,
                BuilderFn builder, bool always_dirty) {
    TargetBuilder tg;
    zmm_tg_init(&tg, g, output);
    zmm_tg_set_builder(&tg, builder);
    if (always_dirty) zmm_tg_set_always_dirty(&tg);
    zmm_tg_add_src(&tg, sources, num_sources);
    zmm_tg_add_dep(&tg, deps, num_deps);
}

void zmm_bg_add_phony(BuildGraph* g, const StringView* sources,
                      usize num_sources, StringView output,
                      const StringView* deps, usize num_deps, BuilderFn builder,
                      bool always_dirty) {
    zmm_bg_add(g, sources, num_sources, output, deps, num_deps, builder,
               always_dirty);
    g->nodes[get_or_put_node(g, output)].flags |= NODE_FLAG_PHONY;
}

// --- TargetBuilder API ---

void zmm_tg_init(TargetBuilder* tg, BuildGraph* g, StringView output) {
    NodeId id = get_or_put_node(g, output);
    g->nodes[id].flags |= NODE_FLAG_IS_TARGET;
    tg->g = g;
    tg->id = id;
}

void zmm_tg_init_phony(TargetBuilder* tg, BuildGraph* g, StringView output) {
    zmm_tg_init(tg, g, output);
    g->nodes[tg->id].flags |= NODE_FLAG_PHONY;
}

void zmm_tg_set_builder(TargetBuilder* tg, BuilderFn builder) {
    tg->g->nodes[tg->id].builder = builder;
}

void zmm_tg_set_phony(TargetBuilder* tg) {
    tg->g->nodes[tg->id].flags |= NODE_FLAG_PHONY;
}

void zmm_tg_set_always_dirty(TargetBuilder* tg) {
    tg->g->nodes[tg->id].flags |= NODE_FLAG_ALWAYS_DIRTY;
}

void zmm_tg_add_src(TargetBuilder* tb, const StringView* sources, usize count) {
    for (usize i = 0; i < count; ++i) {
        StringView duped = bg_dupe_str(tb->g, sources[i]);
        arrpush(tb->g->nodes[tb->id].sources, duped);
        add_dependency_edge(tb->g, tb->id, sources[i]);
    }
}

void zmm_tg_add_dep(TargetBuilder* tb, const StringView* deps, usize count) {
    for (usize i = 0; i < count; ++i) {
        StringView duped = bg_dupe_str(tb->g, deps[i]);
        arrpush(tb->g->nodes[tb->id].deps, duped);
        add_dependency_edge(tb->g, tb->id, deps[i]);
    }
}

// --- Phase 1: Planning / Dirty Checking ---

static inline bool is_dirty_stat(const struct stat* a_stat,
                                 const struct stat* b_stat) {
    long a_sec = a_stat->st_mtime;
    long b_sec = b_stat->st_mtime;

#ifdef _WIN32
    long a_nsec = 0;
    long b_nsec = 0;
#elif defined(__APPLE__)
    long a_nsec = a_stat->st_mtimespec.tv_nsec;
    long b_nsec = b_stat->st_mtimespec.tv_nsec;
#else
    // Linux and standard POSIX systems
    long a_nsec = a_stat->st_mtimensec;
    long b_nsec = b_stat->st_mtimensec;
#endif

    if (b_sec > a_sec) return true;
    if (a_sec == b_sec && b_nsec > a_nsec) return true;
    return false;
}

// Recursively evaluate dirty state (Bottom-Up)
static bool eval_node_dirty(BuildGraph* g, NodeId id) {
    Node* n = &g->nodes[id];
    if (n->eval_visited) return n->is_dirty;
    n->eval_visited = true;

    bool dirty = false;

    // 1. Check Always Dirty
    if (n->flags & NODE_FLAG_ALWAYS_DIRTY) {
        dirty = true;
    }

    // 2. Evaluate all dependencies first
    for (usize i = 0; i < arrlenu(n->waits_on); ++i) {
        if (eval_node_dirty(g, n->waits_on[i])) {
            dirty = true;  // Bubble up dirty state
        }
    }

    if (dirty) {
        n->is_dirty = true;
        return true;
    }

    // 3. If it's a Phony target and dependencies were clean, it stays clean
    if (n->flags & NODE_FLAG_PHONY) {
        n->is_dirty = false;
        return false;
    }

    // 4. Standard File Stat checks
    struct stat out_stat;
    if (stat_strview(n->output, &out_stat) != 0) {
        n->is_dirty = true;  // Output doesn't exist
        return true;
    }

    // Check Sources
    for (usize i = 0; i < arrlenu(n->sources); ++i) {
        struct stat in_stat;
        if (stat_strview(n->sources[i], &in_stat) != 0 ||
            is_dirty_stat(&out_stat, &in_stat)) {
            n->is_dirty = true;
            return true;
        }
    }

    // Check Deps
    for (usize i = 0; i < arrlenu(n->deps); ++i) {
        struct stat in_stat;
        if (stat_strview(n->deps[i], &in_stat) != 0 ||
            is_dirty_stat(&out_stat, &in_stat)) {
            n->is_dirty = true;
            return true;
        }
    }

    n->is_dirty = false;
    return false;
}

static void plan_subgraph(BuildGraph* g, NodeId start_id) {
    NodeId* stack = NULL;
    arrpush(stack, start_id);

    while (arrlen(stack) > 0) {
        NodeId id = arrpop(stack);
        Node* n = &g->nodes[id];

        if (n->is_in_subgraph) continue;
        n->is_in_subgraph = true;

        for (usize i = 0; i < arrlenu(n->waits_on); ++i) {
            if (!g->nodes[n->waits_on[i]].is_in_subgraph) {
                arrpush(stack, n->waits_on[i]);
            }
        }
    }
    arrfree(stack);
}

int zmm_bg_prepare(BuildGraph* g, const StringView* targets,
                   usize num_targets) {
    khash_t(node_map)* h = (khash_t(node_map)*)g->node_map;

    // Reset state
    for (usize i = 0; i < arrlenu(g->nodes); ++i) {
        g->nodes[i].is_in_subgraph = false;
        g->nodes[i].eval_visited = false;
        g->nodes[i].is_dirty = false;
        atomic_store(&g->nodes[i].pending_deps, 0);
    }

    // 1. Mark subgraph
    for (usize i = 0; i < num_targets; ++i) {
        khint_t k = kh_get(node_map, h, targets[i]);
        if (k == kh_end(h)) return -1;  // Target not found
        plan_subgraph(g, kh_val(h, k));
    }

    // 2. Evaluate dirty state bottom-up
    for (usize i = 0; i < arrlenu(g->nodes); ++i) {
        if (g->nodes[i].is_in_subgraph) {
            eval_node_dirty(g, (NodeId)i);
        }
    }

    // 3. Setup execution plan (Only tracking dirty nodes waiting on dirty
    // dependencies)
    u32 total_dirty = 0;
    for (usize i = 0; i < arrlenu(g->nodes); ++i) {
        Node* n = &g->nodes[i];
        if (n->is_in_subgraph && n->is_dirty) {
            total_dirty++;
            u32 p_deps = 0;
            for (usize j = 0; j < arrlenu(n->waits_on); ++j) {
                if (g->nodes[n->waits_on[j]].is_dirty) {
                    p_deps++;
                }
            }
            atomic_store(&n->pending_deps, p_deps);
        }
    }

    atomic_store(&g->remaining_nodes, total_dirty);
    return 0;
}

// --- Phase 2: Querying ---

bool zmm_bg_is_dirty(BuildGraph* g, StringView target) {
    khash_t(node_map)* h = (khash_t(node_map)*)g->node_map;
    khint_t k = kh_get(node_map, h, target);
    if (k == kh_end(h)) return false;

    return g->nodes[kh_val(h, k)].is_dirty;
}

// --- Phase 3: Concurrent Execution ---

typedef struct {
    BuildGraph* graph;
    TaskQueue* queue;
    _Atomic bool* has_error;
    pthread_mutex_t* done_lock;
    pthread_cond_t* done_cond;
} WorkerCtx;

static void* worker_fn(void* arg) {
    WorkerCtx* ctx = (WorkerCtx*)arg;
    NodeId node_id;

    while (queue_pop(ctx->queue, &node_id)) {
        if (atomic_load_explicit(ctx->has_error, memory_order_acquire)) {
            u32 rem = atomic_fetch_sub(&ctx->graph->remaining_nodes, 1);
            if (rem == 1) {
                pthread_mutex_lock(ctx->done_lock);
                pthread_cond_signal(ctx->done_cond);
                pthread_mutex_unlock(ctx->done_lock);
            }
            continue;
        }

        Node* node = &ctx->graph->nodes[node_id];

        // Execute the node's builder if it's a target
        if ((node->flags & NODE_FLAG_IS_TARGET) && node->builder) {
            int res = node->builder(node->sources, arrlenu(node->sources),
                                    node->output);
            if (res != 0) {
                atomic_store_explicit(ctx->has_error, true,
                                      memory_order_release);
            }
        }

        // Notify dirty dependents
        for (usize i = 0; i < arrlenu(node->dependents); ++i) {
            NodeId dep_id = node->dependents[i];
            Node* dep_node = &ctx->graph->nodes[dep_id];

            if (dep_node->is_in_subgraph && dep_node->is_dirty) {
                u32 prev = atomic_fetch_sub(&dep_node->pending_deps, 1);
                if (prev == 1) {  // We were the last dirty dependency
                    queue_push(ctx->queue, dep_id);
                }
            }
        }

        u32 rem = atomic_fetch_sub(&ctx->graph->remaining_nodes, 1);
        if (rem == 1) {
            pthread_mutex_lock(ctx->done_lock);
            pthread_cond_signal(ctx->done_cond);
            pthread_mutex_unlock(ctx->done_lock);
        }
    }
    return NULL;
}

int zmm_bg_exec(BuildGraph* g) {
    u32 total_nodes = atomic_load(&g->remaining_nodes);
    if (total_nodes == 0) return 0;  // Everything is clean

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

    for (u32 i = 0; i < num_threads; ++i) {
        ctxs[i] = (WorkerCtx){.graph = g,
                              .queue = &queue,
                              .has_error = &has_error,
                              .done_lock = &done_lock,
                              .done_cond = &done_cond};
        pthread_create(&threads[i], NULL, worker_fn, &ctxs[i]);
    }

    // Seed the queue with dirty leaf nodes
    for (usize i = 0; i < arrlenu(g->nodes); ++i) {
        Node* n = &g->nodes[i];
        if (n->is_in_subgraph && n->is_dirty &&
            atomic_load(&n->pending_deps) == 0) {
            queue_push(&queue, (NodeId)i);
        }
    }

    pthread_mutex_lock(&done_lock);
    while (atomic_load(&g->remaining_nodes) > 0) {
        pthread_cond_wait(&done_cond, &done_lock);
    }
    pthread_mutex_unlock(&done_lock);

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
