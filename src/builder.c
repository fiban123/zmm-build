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
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "cpu.h"
#include "str.h"
#include "vec.h"

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || \
    defined(__NetBSD__)
// macOS and BSDs use st_mtimespec
#define GET_MTIME_NSEC(st_ptr) ((st_ptr)->st_mtimespec.tv_nsec)
#elif defined(__linux__) || defined(__CYGWIN__) || \
    (defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200809L)
// Linux and modern POSIX.1-2008 use st_mtim
#define GET_MTIME_NSEC(st_ptr) ((st_ptr)->st_mtim.tv_nsec)
#else
// Safe fallback for older systems that only support second-level precision
#define GET_MTIME_NSEC(st_ptr) 0
#endif

// --- Constants & Types ---

#define NODE_FLAG_PHONY (1 << 0)
#define NODE_FLAG_ALWAYS_DIRTY (1 << 1)
#define NODE_FLAG_IS_TARGET (1 << 2)

typedef usize NodeId;

// --- khash Map Implementation ---

static inline khint_t hash_strview(StringView s) {
    khint_t h = 2166136261U;
    for (usize i = 0; i < s.len; ++i) {
        h *= 16777619U;
        h ^= (u8)s.ptr[i];
    }
    return h;
}

KHASH_INIT(node_map, StringView, NodeId, 1, hash_strview, zmm_str_eq)

// --- Graph Data Structures ---

typedef struct Node {
    StringView output;

    vec(StringView) sources;
    vec(StringView) deps;

    vec(NodeId) dependents;
    vec(NodeId) waits_on;

    BuilderFn builder;
    u8 flags;

    bool is_in_subgraph;
    bool eval_visited;
    bool is_dirty;
    _Atomic u32 pending_deps;
} Node;

// --- Task Queue Implementation ---

typedef struct {
    NodeId* buffer;
    usize capacity;
    usize head;
    usize tail;
    usize count;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    bool shutdown;
} TaskQueue;

static int queue_init(TaskQueue* q, usize capacity) {
    q->buffer = (NodeId*)malloc(capacity * sizeof(NodeId));
    if (!q->buffer) return 1;
    q->capacity = capacity;
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    q->shutdown = false;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    return 0;
}

static void queue_free(TaskQueue* q) {
    if (q->buffer) free(q->buffer);
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

static StringView bg_dupe_str(BuildGraph* g, StringView v) {
    if (!v.ptr || v.len == 0) return (StringView){NULL, 0};
    char* ptr = (char*)malloc(v.len + 1);
    if (!ptr) return (StringView){NULL, 0};
    memcpy(ptr, v.ptr, v.len);
    ptr[v.len] = '\0';
    vecpush(g->internal_allocs, ptr);
    return (StringView){ptr, v.len};
}

#define MAX_TEMP_PATH 4096
static _Thread_local char tl_path_buf[MAX_TEMP_PATH];

static inline char* strview_to_temp_cstr(StringView view, bool* out_is_heap) {
    *out_is_heap = false;
    if (!view.ptr) return NULL;
    char* cstr = tl_path_buf;
    if (view.len < MAX_TEMP_PATH) {
        memcpy(cstr, view.ptr, view.len);
        cstr[view.len] = '\0';
    } else {
        *out_is_heap = true;
        cstr = (char*)malloc(view.len + 1);
        if (!cstr) return NULL;
        memcpy(cstr, view.ptr, view.len);
        cstr[view.len] = '\0';
    }
    return cstr;
}

static inline int stat_strview(StringView path, struct stat* out_stat) {
    bool is_heap;
    char* cstr = strview_to_temp_cstr(path, &is_heap);
    if (!cstr) return 1;
    int res = stat(cstr, out_stat);
    if (is_heap) free(cstr);
    return res;
}

// --- Graph Initialization & Teardown ---

API void zmm_bg_init(BuildGraph* g) {
    if (!g) return;
    g->nodes = NULL;
    g->node_map = kh_init(node_map);
    g->internal_allocs = NULL;
    g->remaining_dirty = 0;
}

API void zmm_bg_free(BuildGraph* g) {
    if (!g) return;

    for (usize i = 0; i < vecsize(g->nodes); ++i) {
        vecfree(g->nodes[i].sources);
        vecfree(g->nodes[i].deps);
        vecfree(g->nodes[i].dependents);
        vecfree(g->nodes[i].waits_on);
    }
    vecfree(g->nodes);

    if (g->node_map) {
        kh_destroy(node_map, (khash_t(node_map)*)g->node_map);
    }

    for (usize i = 0; i < vecsize(g->internal_allocs); ++i) {
        free(g->internal_allocs[i]);
    }
    vecfree(g->internal_allocs);
}

static NodeId get_or_put_node(BuildGraph* g, StringView path) {
    khash_t(node_map)* h = (khash_t(node_map)*)g->node_map;

    khint_t k = kh_get(node_map, h, path);
    if (k != kh_end(h)) {
        return kh_val(h, k);
    }

    StringView perm_key = bg_dupe_str(g, path);

    NodeId id = (NodeId)vecsize(g->nodes);
    Node n = {0};
    n.output = perm_key;
    vecpush(g->nodes, n);

    int ret;
    k = kh_put(node_map, h, perm_key, &ret);
    kh_val(h, k) = id;

    return id;
}

static void add_dependency_edge(BuildGraph* g, NodeId target_id,
                                StringView dep_path) {
    NodeId dep_id = get_or_put_node(g, dep_path);
    vecpush(g->nodes[dep_id].dependents, target_id);
    vecpush(g->nodes[target_id].waits_on, dep_id);
}

// --- Public Graph Addition API ---

API int zmm_bg_add(BuildGraph* g, const StringView* sources, usize num_sources,
                   StringView output, const StringView* deps, usize num_deps,
                   BuilderFn builder, bool always_dirty) {
    TargetBuilder tg;
    zmm_tg_init(&tg, g, output);
    zmm_tg_set_builder(&tg, builder);
    if (always_dirty) zmm_tg_set_always_dirty(&tg);
    if (zmm_tg_add_src(&tg, sources, num_sources)) return 1;
    if (num_deps > 0 && zmm_tg_add_dep(&tg, deps, num_deps) != 0) return 1;

    return 0;
}

API int zmm_bg_add_phony(BuildGraph* g, const StringView* sources,
                         usize num_sources, StringView output,
                         const StringView* deps, usize num_deps,
                         BuilderFn builder, bool always_dirty) {
    if (zmm_bg_add(g, sources, num_sources, output, deps, num_deps, builder,
                   always_dirty)) {
        return 1;
    }

    g->nodes[get_or_put_node(g, output)].flags |= NODE_FLAG_PHONY;
    return 0;
}

// --- TargetBuilder API ---

API void zmm_tg_init(TargetBuilder* tg, BuildGraph* g, StringView output) {
    if (!tg || !g) return;
    NodeId id = get_or_put_node(g, output);
    g->nodes[id].flags |= NODE_FLAG_IS_TARGET;
    tg->g = g;
    tg->id = id;
}

API void zmm_tg_set_builder(TargetBuilder* tg, BuilderFn builder) {
    tg->g->nodes[tg->id].builder = builder;
}

API void zmm_tg_set_phony(TargetBuilder* tg) {
    tg->g->nodes[tg->id].flags |= NODE_FLAG_PHONY;
}

API void zmm_tg_set_always_dirty(TargetBuilder* tg) {
    tg->g->nodes[tg->id].flags |= NODE_FLAG_ALWAYS_DIRTY;
}

API int zmm_tg_add_src(TargetBuilder* tb, const StringView* sources,
                       usize count) {
    for (usize i = 0; i < count; ++i) {
        StringView duped = bg_dupe_str(tb->g, sources[i]);
        if (!duped.ptr) return 1;  // Allocation failure
        vecpush(tb->g->nodes[tb->id].sources, duped);
        add_dependency_edge(tb->g, tb->id, sources[i]);
    }
    return 0;
}

API int zmm_tg_add_dep(TargetBuilder* tb, const StringView* deps, usize count) {
    for (usize i = 0; i < count; ++i) {
        StringView duped = bg_dupe_str(tb->g, deps[i]);
        if (!duped.ptr) return 1;  // Allocation failure
        vecpush(tb->g->nodes[tb->id].deps, duped);
        add_dependency_edge(tb->g, tb->id, deps[i]);
    }
    return 0;
}

// --- Phase 1: Planning / Dirty Checking ---

static inline bool is_dirty_stat(const struct stat* a_stat,
                                 const struct stat* b_stat) {
    long a_sec = a_stat->st_mtime;
    long b_sec = b_stat->st_mtime;

    long a_nsec = GET_MTIME_NSEC(a_stat);
    long b_nsec = GET_MTIME_NSEC(b_stat);

    if (b_sec > a_sec) return true;
    if (a_sec == b_sec && b_nsec > a_nsec) return true;
    return false;
}

static bool eval_node_dirty(BuildGraph* g, NodeId id) {
    Node* n = &g->nodes[id];
    if (n->eval_visited) return n->is_dirty;
    n->eval_visited = true;

    bool dirty = false;

    if (n->flags & NODE_FLAG_ALWAYS_DIRTY) {
        dirty = true;
    }

    for (usize i = 0; i < vecsize(n->waits_on); ++i) {
        if (eval_node_dirty(g, n->waits_on[i])) {
            dirty = true;
        }
    }

    if (dirty) {
        n->is_dirty = true;
        return true;
    }

    if (n->flags & NODE_FLAG_PHONY) {
        n->is_dirty = false;
        return false;
    }

    struct stat out_stat;
    if (stat_strview(n->output, &out_stat) != 0) {
        n->is_dirty = true;
        return true;
    }

    for (usize i = 0; i < vecsize(n->sources); ++i) {
        struct stat in_stat;
        if (stat_strview(n->sources[i], &in_stat) != 0 ||
            is_dirty_stat(&out_stat, &in_stat)) {
            n->is_dirty = true;
            return true;
        }
    }

    for (usize i = 0; i < vecsize(n->deps); ++i) {
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
    vecpush(stack, start_id);

    while (vecsize(stack) > 0) {
        NodeId id = vecpop(stack);
        Node* n = &g->nodes[id];

        if (n->is_in_subgraph) continue;
        n->is_in_subgraph = true;

        for (usize i = 0; i < vecsize(n->waits_on); ++i) {
            if (!g->nodes[n->waits_on[i]].is_in_subgraph) {
                vecpush(stack, n->waits_on[i]);
            }
        }
    }
    vecfree(stack);
}

API int zmm_bg_prepare(BuildGraph* g, const StringView* targets,
                       usize num_targets) {
    khash_t(node_map)* h = (khash_t(node_map)*)g->node_map;

    for (usize i = 0; i < vecsize(g->nodes); ++i) {
        g->nodes[i].is_in_subgraph = false;
        g->nodes[i].eval_visited = false;
        g->nodes[i].is_dirty = false;
        atomic_store(&g->nodes[i].pending_deps, 0);
    }

    for (usize i = 0; i < num_targets; ++i) {
        khint_t k = kh_get(node_map, h, targets[i]);
        if (k == kh_end(h)) return -1;
        plan_subgraph(g, kh_val(h, k));
    }

    for (usize i = 0; i < vecsize(g->nodes); ++i) {
        if (g->nodes[i].is_in_subgraph) {
            eval_node_dirty(g, (NodeId)i);
        }
    }

    usize total_dirty = 0;
    for (usize i = 0; i < vecsize(g->nodes); ++i) {
        Node* n = &g->nodes[i];
        if (n->is_in_subgraph && n->is_dirty) {
            total_dirty++;
            u32 p_deps = 0;
            for (usize j = 0; j < vecsize(n->waits_on); ++j) {
                if (g->nodes[n->waits_on[j]].is_dirty) {
                    p_deps++;
                }
            }
            atomic_store(&n->pending_deps, p_deps);
        }
    }

    g->remaining_dirty = total_dirty;
    return 0;
}

// --- Phase 2: Querying ---

API bool zmm_bg_is_dirty(BuildGraph* g, StringView target) {
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
        // If an error occurred elsewhere, drain the queue without executing
        if (atomic_load_explicit(ctx->has_error, memory_order_acquire)) {
            continue;
        }

        Node* node = &ctx->graph->nodes[node_id];
        int res = 0;

        // Execute the node's builder if it's a target
        if ((node->flags & NODE_FLAG_IS_TARGET) && node->builder) {
            res = node->builder(node->sources, vecsize(node->sources),
                                node->output);
        }

        if (res != 0) {
            // Signal global failure
            atomic_store_explicit(ctx->has_error, true, memory_order_release);

            // Wake up main thread immediately to abort
            pthread_mutex_lock(ctx->done_lock);
            pthread_cond_broadcast(ctx->done_cond);
            pthread_mutex_unlock(ctx->done_lock);
            continue;
        }

        // Notify dirty dependents (Success Path)
        for (usize i = 0; i < vecsize(node->dependents); ++i) {
            NodeId dep_id = node->dependents[i];
            Node* dep_node = &ctx->graph->nodes[dep_id];

            if (dep_node->is_in_subgraph && dep_node->is_dirty) {
                u32 prev = atomic_fetch_sub(&dep_node->pending_deps, 1);
                if (prev == 1) {
                    queue_push(ctx->queue, dep_id);
                }
            }
        }

        // Decrement remaining global tasks
        pthread_mutex_lock(ctx->done_lock);
        ctx->graph->remaining_dirty--;
        if (ctx->graph->remaining_dirty == 0) {
            pthread_cond_broadcast(ctx->done_cond);
        }
        pthread_mutex_unlock(ctx->done_lock);
    }
    return NULL;
}

API int zmm_bg_exec(BuildGraph* g) {
    usize total_nodes = g->remaining_dirty;
    if (total_nodes == 0) return 0;  // Everything is clean

    TaskQueue queue;
    if (queue_init(&queue, total_nodes) != 0) return -1;

    _Atomic bool has_error;
    atomic_init(&has_error, false);

    pthread_mutex_t done_lock;
    pthread_cond_t done_cond;
    pthread_mutex_init(&done_lock, NULL);
    pthread_cond_init(&done_cond, NULL);

    u32 num_threads = zmm_cpu_thread_count();
    if (num_threads == 0) num_threads = 1;  // Fallback

    pthread_t* threads = (pthread_t*)malloc(num_threads * sizeof(pthread_t));
    WorkerCtx* ctxs = (WorkerCtx*)malloc(num_threads * sizeof(WorkerCtx));

    if (!threads || !ctxs) {
        if (threads) free(threads);
        if (ctxs) free(ctxs);
        queue_free(&queue);
        return 1;
    }

    for (u32 i = 0; i < num_threads; ++i) {
        ctxs[i] = (WorkerCtx){.graph = g,
                              .queue = &queue,
                              .has_error = &has_error,
                              .done_lock = &done_lock,
                              .done_cond = &done_cond};
        pthread_create(&threads[i], NULL, worker_fn, &ctxs[i]);
    }

    // Seed the queue with dirty leaf nodes
    for (usize i = 0; i < vecsize(g->nodes); ++i) {
        Node* n = &g->nodes[i];
        if (n->is_in_subgraph && n->is_dirty &&
            atomic_load(&n->pending_deps) == 0) {
            queue_push(&queue, (NodeId)i);
        }
    }

    // Wait for completion OR error abort
    pthread_mutex_lock(&done_lock);
    while (g->remaining_dirty > 0 && !atomic_load(&has_error)) {
        pthread_cond_wait(&done_cond, &done_lock);
    }
    pthread_mutex_unlock(&done_lock);

    // Shutdown queue to flush remaining tasks and unblock workers waiting on
    // pop
    queue_shutdown(&queue);

    for (u32 i = 0; i < num_threads; ++i) {
        pthread_join(threads[i], NULL);
    }

    free(threads);
    free(ctxs);
    pthread_mutex_destroy(&done_lock);
    pthread_cond_destroy(&done_cond);
    queue_free(&queue);

    return atomic_load(&has_error) ? 1 : 0;
}
