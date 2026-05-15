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
