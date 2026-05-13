#include "cpu.h"

#include <unistd.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "slice.h"

void zmm_cpu_init(CpuInfo* info) {
    if (!info) return;
    info->x86 = GetX86Info();
    info->cache = GetX86CacheInfo();
}

i32 zmm_cpu_cache_size(const CpuInfo* info, int level, CacheType cache_type) {
    if (!info) return 0;
    for (int i = 0; i < info->cache.size; ++i) {
        if (info->cache.levels[i].level == level &&
            info->cache.levels[i].cache_type == cache_type) {
            return info->cache.levels[i].cache_size;
        }
    }
    return 0;
}

i32 zmm_cpu_l1d_size(const CpuInfo* info) {
    // We specifically want L1 Data, not Instruction
    return zmm_cpu_cache_size(info, 1, CPU_FEATURE_CACHE_DATA);
}

i32 zmm_cpu_l1i_size(const CpuInfo* info) {
    return zmm_cpu_cache_size(info, 1, CPU_FEATURE_CACHE_INSTRUCTION);
}

i32 zmm_cpu_l2_size(const CpuInfo* info) {
    int size = zmm_cpu_cache_size(info, 2, CPU_FEATURE_CACHE_UNIFIED);
    if (size == 0) {
        size = zmm_cpu_cache_size(info, 2, CPU_FEATURE_CACHE_DATA);
    }
    return size;
}

i32 zmm_cpu_l3_size(const CpuInfo* info) {
    // L3 is universally Unified
    return zmm_cpu_cache_size(info, 3, CPU_FEATURE_CACHE_UNIFIED);
}

i32 zmm_cpu_thread_count() {
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

SliceU8 zmm_cpu_brand_string(const CpuInfo* info) {
    if (!info) return NullSliceU8;

    usize len = strlen(info->x86.brand_string);

    SliceU8 s = {.ptr = malloc(len), .len = len};

    memcpy(s.ptr, info->x86.brand_string, len);

    return s;
}

SliceU8 zmm_cpu_vendor(const CpuInfo* info) {
    if (!info) return NullSliceU8;

    usize len = strlen(info->x86.vendor);

    SliceU8 s = {.ptr = malloc(len), .len = len};

    memcpy(s.ptr, info->x86.vendor, len);

    return s;
}

SliceU8 zmm_cpu_uarch_str(const CpuInfo* info) {
    if (!info) return NullSliceU8;
    const char* nul_s =
        GetX86MicroarchitectureName(GetX86Microarchitecture(&info->x86));

    usize len = strlen(nul_s);

    SliceU8 s = {.ptr = malloc(len), .len = len};

    memcpy(s.ptr, nul_s, len);

    return s;
}

i32 zmm_cpu_family(const CpuInfo* info) { return info ? info->x86.family : 0; }

i32 zmm_cpu_model(const CpuInfo* info) { return info ? info->x86.model : 0; }

i32 zmm_cpu_stepping(const CpuInfo* info) {
    return info ? info->x86.stepping : 0;
}
