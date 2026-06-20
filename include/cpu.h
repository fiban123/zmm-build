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
#include "str.h"

typedef struct {
    X86Info x86;
    CacheInfo cache;
} CpuInfo;

API void zmm_cpu_init(CpuInfo* info);

#define zmm_cpu_supports(info_ptr, feat) ((info_ptr)->x86.features.feat)

API i32 zmm_cpu_cache_size(const CpuInfo* info, int level,
                           CacheType cache_type);

API i32 zmm_cpu_l1d_size(const CpuInfo* info);

API i32 zmm_cpu_l1i_size(const CpuInfo* info);

API i32 zmm_cpu_l2_size(const CpuInfo* info);

API i32 zmm_cpu_l3_size(const CpuInfo* info);

API i32 zmm_cpu_thread_count();

API String zmm_cpu_brand_string(const CpuInfo* info);

API String zmm_cpu_vendor(const CpuInfo* info);

API String zmm_cpu_uarch_str(const CpuInfo* info);

API i32 zmm_cpu_family(const CpuInfo* info);

API i32 zmm_cpu_model(const CpuInfo* info);

API i32 zmm_cpu_stepping(const CpuInfo* info);
