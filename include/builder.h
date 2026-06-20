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

#include "str.h"

typedef int (*BuilderFn)(const StringView* sources, usize num_sources,
                         StringView output);

// Opaque types
typedef struct BuildGraph BuildGraph;
typedef struct TargetBuilder TargetBuilder;

// --- Graph Management ---
void zmm_bg_init(BuildGraph* g);
void zmm_bg_free(BuildGraph* g);

// --- Phase 1: Planning ---
// Evaluates the graph, stats files, and builds the execution plan.
int zmm_bg_prepare(BuildGraph* g, const StringView* targets, usize num_targets);

// --- Phase 2: Querying ---
// Returns true if the target is scheduled to be rebuilt (O(1) lookup).
// Must be called AFTER zmm_bg_prepare.
bool zmm_bg_is_dirty(BuildGraph* g, StringView target);

// --- Phase 3: Execution ---
// Executes the plan concurrently. No disk I/O for timestamps happens here.
int zmm_bg_exec(BuildGraph* g);

// --- Direct Addition API ---
void zmm_bg_add(BuildGraph* g, const StringView* sources, usize num_sources,
                StringView output, const StringView* deps, usize num_deps,
                BuilderFn builder, bool always_dirty);
void zmm_bg_add_phony(BuildGraph* g, const StringView* sources,
                      usize num_sources, StringView output,
                      const StringView* deps, usize num_deps, BuilderFn builder,
                      bool always_dirty);

// --- TargetBuilder API ---
void zmm_tg_init(TargetBuilder* tg, BuildGraph* g, StringView output);
void zmm_tg_init_phony(TargetBuilder* tg, BuildGraph* g, StringView output);

// Target Modifiers
void zmm_tg_set_builder(TargetBuilder* tg, BuilderFn builder);
void zmm_tg_set_phony(TargetBuilder* tg);
void zmm_tg_set_always_dirty(TargetBuilder* tg);

void zmm_tg_add_src(TargetBuilder* tb, const StringView* sources, usize count);
void zmm_tg_add_dep(TargetBuilder* tb, const StringView* deps, usize count);
