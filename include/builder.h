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

#include "export.h"
#include "str.h"

typedef int (*BuilderFn)(const StringView* sources, usize num_sources,
                         StringView output);

struct Node;

typedef struct {
    struct Node* nodes;
    void* node_map;
    char** internal_allocs;

    // holds the number of targets which are
    // dirty.
    usize remaining_dirty;
} BuildGraph;

typedef struct TargetBuilder {
    BuildGraph* g;
    usize id;
} TargetBuilder;

API void zmm_bg_init(BuildGraph* g);
API void zmm_bg_free(BuildGraph* g);

API int zmm_bg_prepare(BuildGraph* g, const StringView* targets,
                       usize num_targets);

API bool zmm_bg_is_dirty(BuildGraph* g, StringView target);

API int zmm_bg_exec(BuildGraph* g);

API int zmm_bg_add(BuildGraph* g, const StringView* sources, usize num_sources,
                   StringView output, const StringView* deps, usize num_deps,
                   BuilderFn builder, bool always_dirty);

API int zmm_bg_add_phony(BuildGraph* g, const StringView* sources,
                         usize num_sources, StringView output,
                         const StringView* deps, usize num_deps,
                         BuilderFn builder, bool always_dirty);

API void zmm_tg_init(TargetBuilder* tg, BuildGraph* g, StringView output);

API void zmm_tg_set_builder(TargetBuilder* tg, BuilderFn builder);
API void zmm_tg_set_phony(TargetBuilder* tg);
API void zmm_tg_set_always_dirty(TargetBuilder* tg);

API int zmm_tg_add_src(TargetBuilder* tb, const StringView* sources,
                       usize count);
API int zmm_tg_add_dep(TargetBuilder* tb, const StringView* deps, usize count);
