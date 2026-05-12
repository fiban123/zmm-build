#include "ztime.h"

#include <time.h>

u64 zmm_time_ms() {
    struct timespec spec;
    timespec_get(&spec, TIME_UTC);

    return (spec.tv_sec * 1000) + (spec.tv_nsec / 1000000);
}

u64 zmm_time_us() {
    struct timespec spec;
    timespec_get(&spec, TIME_UTC);

    return (spec.tv_sec * 1000000) + (spec.tv_nsec / 1000);
}

u64 zmm_time_ns() {
    struct timespec spec;
    timespec_get(&spec, TIME_UTC);

    return (spec.tv_sec * 1000000000) + (spec.tv_nsec);
}
