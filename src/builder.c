#include "builder.h"

#include <pthread.h>
#include <stb/stb_ds.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "arr.h"

typedef u32 NodeId;

// stb_ds string map structure
typedef struct NodeMap {
    char* key;
    NodeId value;
} NodeMap;

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
    pthread_mutex_init(&q->lock, nullptr);
    pthread_cond_init(&q->not_empty, nullptr);
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

// --- Graph Utilities ---

void zmm_builder_init(BuildGraph* g) {
    g->nodes = arrinit;
    g->node_map = stringhm_init;
}

void zmm_builder_free(BuildGraph* g) {
    if (!g) return;

    for (usize i = 0; i < arrlenu(g->nodes); ++i) {
        arrfree(g->nodes[i].sources);
        arrfree(g->nodes[i].deps);
        arrfree(g->nodes[i].dependents);
        arrfree(g->nodes[i].waits_on);
    }
    arrfree(g->nodes);

    for (usize i = 0; i < shlenu(g->node_map); ++i) {
        free(g->node_map[i]
                 .key);  // Free the allocated C-strings used as map keys
    }
    shfree(g->node_map);
}

static NodeId get_or_put_node(BuildGraph* g, SliceCU8 path) {
    char* cstr = slice_to_cstr(path);
    ptrdiff_t index = shgeti(g->node_map, cstr);

    if (index >= 0) {
        free(cstr);  // Already exists, free the temp string
        return g->node_map[index].value;
    }

    // New node
    NodeId id = (NodeId)arrlenu(g->nodes);
    Node n = {0};
    n.output = path;
    arrpush(g->nodes, n);

    // Put in map. The map uses `cstr` directly as the key pointer, so we DO NOT
    // free it here. It will be freed in zmm_builder_graph_deinit.
    shput(g->node_map, cstr, id);

    return id;
}

void zmm_builder_add(BuildGraph* g, SliceCU8 const* sources, usize num_sources,
                     SliceCU8 output, SliceCU8 const* deps, usize num_deps) {
    NodeId out_id = get_or_put_node(g, output);
    g->nodes[out_id].is_target = true;

    for (usize i = 0; i < num_sources; ++i)
        arrpush(g->nodes[out_id].sources, sources[i]);
    for (usize i = 0; i < num_deps; ++i)
        arrpush(g->nodes[out_id].deps, deps[i]);

    SliceCU8 const* lists[2] = {sources, deps};
    usize counts[2] = {num_sources, num_deps};

    for (int list_idx = 0; list_idx < 2; ++list_idx) {
        for (usize i = 0; i < counts[list_idx]; ++i) {
            // Note: get_or_put_node might stretch g->nodes, invalidating direct
            // pointers. Always operate via array indexing!
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

static bool is_dirty(SliceCU8 output, SliceCU8 const* inputs,
                     usize num_inputs) {
    char* out_path = slice_to_cstr(output);
    struct stat out_stat;
    int out_res = stat(out_path, &out_stat);
    free(out_path);

    if (out_res != 0) return true;  // Output doesn't exist

    for (usize i = 0; i < num_inputs; ++i) {
        char* in_path = slice_to_cstr(inputs[i]);
        struct stat in_stat;
        int in_res = stat(in_path, &in_stat);
        free(in_path);

        if (in_res != 0) return true;

        long in_sec, in_nsec, out_sec, out_nsec;

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__)
        // macOS and BSDs
        in_sec = in_stat.st_mtimespec.tv_sec;
        in_nsec = in_stat.st_mtimespec.tv_nsec;
        out_sec = out_stat.st_mtimespec.tv_sec;
        out_nsec = out_stat.st_mtimespec.tv_nsec;
#elif defined(_WIN32) || defined(__MINGW32__)
        // Windows / MinGW
        in_sec = in_stat.st_mtime;
        in_nsec = 0;
        out_sec = out_stat.st_mtime;
        out_nsec = 0;
#else
        // Linux / POSIX
        in_sec = in_stat.st_mtime;
        out_sec = out_stat.st_mtime;

#if defined(st_mtime)
        // If st_mtime is a macro, glibc aliased it to st_mtim.tv_sec
        in_nsec = in_stat.st_mtim.tv_nsec;
        out_nsec = out_stat.st_mtim.tv_nsec;
#else
        // If it's not a macro, glibc fell back to the older variable names
        in_nsec = in_stat.st_mtimensec;
        out_nsec = out_stat.st_mtimensec;
#endif
#endif

        if (in_sec > out_sec) return true;
        if (in_sec == out_sec && in_nsec > out_nsec) return true;
    }
    return false;
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
                is_dirty(node->output, node->sources, arrlenu(node->sources));
            if (!dirty) {
                dirty = is_dirty(node->output, node->deps, arrlenu(node->deps));
            }

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
    return nullptr;
}

static u32 get_system_thread_count(void) {
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (u32)n : 4;
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

int zmm_builder_build(BuildGraph* g, SliceCU8 target, BuilderFn builder) {
    char* target_cstr = slice_to_cstr(target);
    ptrdiff_t target_idx = shgeti(g->node_map, target_cstr);
    free(target_cstr);

    if (target_idx < 0) return 1;  // Target not found
    NodeId target_id = g->node_map[target_idx].value;

    // Reset subgraph state for a clean run
    for (usize i = 0; i < arrlenu(g->nodes); ++i) {
        g->nodes[i].is_in_subgraph = false;
    }

    _Atomic u32 remaining_nodes;
    atomic_init(&remaining_nodes, 0);

    mark_subgraph(g, target_id, &remaining_nodes);

    u32 total_nodes = atomic_load(&remaining_nodes);
    if (total_nodes == 0) return 0;

    // Size the queue to the max possible nodes so push() never has to block
    TaskQueue queue;
    queue_init(&queue, total_nodes);

    _Atomic bool has_error;
    atomic_init(&has_error, false);

    pthread_mutex_t done_lock;
    pthread_cond_t done_cond;
    pthread_mutex_init(&done_lock, nullptr);
    pthread_cond_init(&done_cond, nullptr);

    u32 num_threads = get_system_thread_count();
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
        pthread_create(&threads[i], nullptr, worker_fn, &ctxs[i]);
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
        pthread_join(threads[i], nullptr);
    }

    free(threads);
    free(ctxs);
    pthread_mutex_destroy(&done_lock);
    pthread_cond_destroy(&done_cond);
    queue_free(&queue);

    return atomic_load(&has_error) ? 1 : 0;
}
