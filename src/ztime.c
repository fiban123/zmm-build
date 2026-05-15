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
