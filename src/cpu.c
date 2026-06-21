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

#include "cpu.h"

#include <unistd.h>

#ifdef _WIN32
#include <windows.h>
#endif

API void zmm_cpu_init(CpuInfo* info) {
    if (!info) return;
    info->x86 = GetX86Info();
    info->cache = GetX86CacheInfo();
}

API i32 zmm_cpu_cache_size(const CpuInfo* info, int level,
                           CacheType cache_type) {
    if (!info) return 0;
    for (int i = 0; i < info->cache.size; ++i) {
        if (info->cache.levels[i].level == level &&
            info->cache.levels[i].cache_type == cache_type) {
            return info->cache.levels[i].cache_size;
        }
    }
    return 0;
}

API i32 zmm_cpu_l1d_size(const CpuInfo* info) {
    // We specifically want L1 Data, not Instruction
    return zmm_cpu_cache_size(info, 1, CPU_FEATURE_CACHE_DATA);
}

API i32 zmm_cpu_l1i_size(const CpuInfo* info) {
    return zmm_cpu_cache_size(info, 1, CPU_FEATURE_CACHE_INSTRUCTION);
}

API i32 zmm_cpu_l2_size(const CpuInfo* info) {
    int size = zmm_cpu_cache_size(info, 2, CPU_FEATURE_CACHE_UNIFIED);
    if (size == 0) {
        size = zmm_cpu_cache_size(info, 2, CPU_FEATURE_CACHE_DATA);
    }
    return size;
}

API i32 zmm_cpu_l3_size(const CpuInfo* info) {
    // L3 is universally Unified
    return zmm_cpu_cache_size(info, 3, CPU_FEATURE_CACHE_UNIFIED);
}

API i32 zmm_cpu_thread_count() {
#ifdef _WIN32
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    u32 n = (u32)sysinfo.dwNumberOfProcessors;
    return n > 0 ? n : 4;
#else
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (u32)n : 4;
#endif
}

API String zmm_cpu_brand_string(const CpuInfo* info) {
    usize len = strlen(info->x86.brand_string);

    String s = {malloc(len), len};

    memcpy(s.ptr, info->x86.brand_string, len);

    return s;
}

API String zmm_cpu_vendor(const CpuInfo* info) {
    usize len = strlen(info->x86.vendor);

    String s = {malloc(len), len};

    memcpy(s.ptr, info->x86.vendor, len);

    return s;
}

API String zmm_cpu_uarch_str(const CpuInfo* info) {
    const char* nul_s =
        GetX86MicroarchitectureName(GetX86Microarchitecture(&info->x86));

    usize len = strlen(nul_s);

    String s = {malloc(len), len};

    memcpy(s.ptr, nul_s, len);

    return s;
}

API i32 zmm_cpu_family(const CpuInfo* info) {
    return info ? info->x86.family : 0;
}

API i32 zmm_cpu_model(const CpuInfo* info) {
    return info ? info->x86.model : 0;
}

API i32 zmm_cpu_stepping(const CpuInfo* info) {
    return info ? info->x86.stepping : 0;
}
