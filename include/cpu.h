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

#include <cpu_features/cpu_features_cache_info.h>
#include <cpu_features/cpuinfo_x86.h>

#include "export.h"
#include "num.h"
#include "slice.h"

/**
 * A structure containing CPU feature and cache information.
 * It wraps google/cpu_features structures.
 */
typedef struct {
    X86Info x86;
    CacheInfo cache;
} CpuInfo;

/**
 * Populates the CpuInfo struct with x86 features and cache information.
 * 
 * @param info Pointer to the struct to populate.
 */
API void zmm_cpu_init(CpuInfo* info);

/**
 * Returns whether a specific CPU feature is supported.
 * 
 * @param info_ptr Pointer to a CpuInfo struct.
 * @param feat The feature to check (e.g., avx2, sse4_1).
 */
#define zmm_cpu_supports(info_ptr, feat) ((info_ptr)->x86.features.feat)

/**
 * Returns the cache size in bytes for a given level and type.
 * 
 * @param info Pointer to the CpuInfo struct.
 * @param level Cache level (1, 2, 3).
 * @param cache_type Type of cache (DATA, INSTRUCTION, UNIFIED).
 * @return Cache size in bytes, or 0 if missing.
 */
API i32 zmm_cpu_cache_size(const CpuInfo* info, int level,
                           CacheType cache_type);

/**
 * Returns L1 Data Cache size in bytes.
 * 
 * @param info Pointer to the CpuInfo struct.
 * @return Size in bytes.
 */
API i32 zmm_cpu_l1d_size(const CpuInfo* info);

/**
 * Returns L1 Instruction Cache size in bytes.
 * 
 * @param info Pointer to the CpuInfo struct.
 * @return Size in bytes.
 */
API i32 zmm_cpu_l1i_size(const CpuInfo* info);

/**
 * Returns L2 Unified (or Data) Cache size in bytes.
 * 
 * @param info Pointer to the CpuInfo struct.
 * @return Size in bytes.
 */
API i32 zmm_cpu_l2_size(const CpuInfo* info);

/**
 * Returns L3 Unified Cache size in bytes.
 * 
 * @param info Pointer to the CpuInfo struct.
 * @return Size in bytes.
 */
API i32 zmm_cpu_l3_size(const CpuInfo* info);

/**
 * Returns the number of online logical processors.
 * 
 * @return Logical processor count.
 */
API i32 zmm_cpu_thread_count();

/**
 * Returns the full CPU brand string.
 * 
 * @param info Pointer to the CpuInfo struct.
 * @return Slice containing the brand string.
 * 
 * [FREE] The resulting slice's ptr must be freed.
 */
API SliceU8 zmm_cpu_brand_string(const CpuInfo* info);

/**
 * Returns the CPU Vendor ID string.
 * 
 * @param info Pointer to the CpuInfo struct.
 * @return Slice containing the vendor string.
 * 
 * [FREE] The resulting slice's ptr must be freed.
 */
API SliceU8 zmm_cpu_vendor(const CpuInfo* info);

/**
 * Returns a string representation of the Microarchitecture.
 * 
 * @param info Pointer to the CpuInfo struct.
 * @return Slice containing the uarch string.
 * 
 * [FREE] The resulting slice's ptr must be freed.
 */
API SliceU8 zmm_cpu_uarch_str(const CpuInfo* info);

/**
 * Returns the CPU family ID.
 * 
 * @param info Pointer to the CpuInfo struct.
 * @return Family ID.
 */
API i32 zmm_cpu_family(const CpuInfo* info);

/**
 * Returns the CPU model ID.
 * 
 * @param info Pointer to the CpuInfo struct.
 * @return Model ID.
 */
API i32 zmm_cpu_model(const CpuInfo* info);

/**
 * Returns the CPU stepping ID.
 * 
 * @param info Pointer to the CpuInfo struct.
 * @return Stepping ID.
 */
API i32 zmm_cpu_stepping(const CpuInfo* info);
