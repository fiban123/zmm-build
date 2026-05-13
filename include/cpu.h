#pragma once

#include <cpu_features/cpu_features_cache_info.h>
#include <cpu_features/cpuinfo_x86.h>

#include "export.h"
#include "num.h"
#include "slice.h"

// Transparent wrapper. You have full access to the underlying
// Google structs if you ever need raw info (like TLB entries or ways).
typedef struct {
    X86Info x86;
    CacheInfo cache;
} CpuInfo;

/**
 * Populates the CpuInfo struct with x86 features and cache information.
 */
API void zmm_cpu_init(CpuInfo* info);

/**
 * Returns whether a specific CPU feature is supported.
 */
#define zmm_cpu_supports(info_ptr, feat) ((info_ptr)->x86.features.feat)

/**
 * Returns the cache size in bytes for a given level and type.
 * Returns 0 if missing.
 */
API i32 zmm_cpu_cache_size(const CpuInfo* info, int level,
                           CacheType cache_type);

/**
 * Returns L1 Data Cache size in bytes.
 */
API i32 zmm_cpu_l1d_size(const CpuInfo* info);

/**
 * Returns L1 Instruction Cache size in bytes.
 */
API i32 zmm_cpu_l1i_size(const CpuInfo* info);

/**
 * Returns L2 Unified (or Data) Cache size in bytes.
 */
API i32 zmm_cpu_l2_size(const CpuInfo* info);

/**
 * Returns L3 Unified Cache size in bytes.
 */
API i32 zmm_cpu_l3_size(const CpuInfo* info);

/**
 * Returns the number of online logical processors.
 */
API i32 zmm_cpu_thread_count();

/**
 * Returns the full CPU brand string.
 * NOTE: The resulting slice's ptr must be freed.
 */
API SliceU8 zmm_cpu_brand_string(const CpuInfo* info);

/**
 * Returns the CPU Vendor ID string.
 * NOTE: The resulting slice's ptr must be freed.
 */
API SliceU8 zmm_cpu_vendor(const CpuInfo* info);

/**
 * Returns a string representation of the Microarchitecture.
 * NOTE: The resulting slice's ptr must be freed.
 */
API SliceU8 zmm_cpu_uarch_str(const CpuInfo* info);

/**
 * Returns the CPU family ID.
 */
API i32 zmm_cpu_family(const CpuInfo* info);

/**
 * Returns the CPU model ID.
 */
API i32 zmm_cpu_model(const CpuInfo* info);

/**
 * Returns the CPU stepping ID.
 */
API i32 zmm_cpu_stepping(const CpuInfo* info);
