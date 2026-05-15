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

#include "path.h"

#include "num.h"
#include "slice.h"

// returns the extension of `path` as a slice, or a nullslice, if there is none
SliceCU8 zmm_p_ext(SliceCU8 path) {
    if (path.len == 0) return NullSliceCU8;
    for (usize i = path.len - 1;; i--) {
        char c = path.ptr[i];

        if (c == '.') {
            if (i == 0) break;
            char next = path.ptr[i - 1];
            if (!zmm_p_is_separator(next)) {
                return (SliceCU8){.ptr = path.ptr + i,
                                  .len = path.len - 1 - i + 1};
            }
        }
        if (i == 0) break;
    }

    return NullSliceCU8;
}

SliceCU8 zmm_p_stem(SliceCU8 path) {
    if (path.len == 0) return NullSliceCU8;

    // 1. Find where the basename starts (after the last separator)
    usize basename_start = 0;
    for (usize i = path.len - 1;; i--) {
        if (zmm_p_is_separator(path.ptr[i])) {
            basename_start = i + 1;
            break;
        }
        if (i == 0) break;
    }

    // If the path ends in a separator (e.g., "dir/"), there is no stem
    if (basename_start >= path.len) {
        return NullSliceCU8;
    }

    // 2. Find the last dot in the basename to split the stem from the extension
    // We stop at `i > basename_start` so files like ".hidden" are treated as
    // the full stem rather than an empty stem with a ".hidden" extension.
    for (usize i = path.len - 1; i > basename_start; i--) {
        if (path.ptr[i] == '.') {
            return (SliceCU8){.ptr = path.ptr + basename_start,
                              .len = i - basename_start};
        }
    }

    // 3. No valid extension found, so the entire basename is the stem
    return (SliceCU8){.ptr = path.ptr + basename_start,
                      .len = path.len - basename_start};
}

SliceCU8 zmm_p_strip_ext(SliceCU8 path) {
    if (path.len == 0) return path;

    // 1. Find the last separator to know where the basename starts
    usize basename_start = 0;
    for (usize i = path.len - 1;; i--) {
        if (zmm_p_is_separator(path.ptr[i])) {
            basename_start = i + 1;
            break;
        }
        if (i == 0) break;
    }

    // 2. Look for a dot *after* the last separator
    for (usize i = path.len - 1; i > basename_start; i--) {
        if (path.ptr[i] == '.') {
            // Return everything up to (but not including) the dot
            return (SliceCU8){.ptr = path.ptr, .len = i};
        }
    }

    // 3. No valid extension found, return the whole path
    return path;
}

bool zmm_p_has_ext(SliceCU8 path, SliceCU8 target_ext) {
    SliceCU8 ext = zmm_p_ext(path);

    return slice_eq(ext, target_ext);
}

bool zmm_p_has_exts(SliceCU8 path, const SliceCU8* exts) {
    SliceCU8 ext = zmm_p_ext(path);

    usize i = 0;

    SliceCU8 target_ext = exts[i];
    while (target_ext.ptr) {
        if (slice_eq(ext, target_ext)) {
            return true;
        }
        i++;
        target_ext = exts[i];
    }

    return false;
}

bool zmm_p_is_hidden(SliceCU8 path) {
    // walk backwards, if a dot is found, and the next character
    // to the left is a seperator or doesnt exist, it is hidden

    if (path.len == 0) return false;

    for (usize i = path.len - 1;; i--) {
        char c = path.ptr[i];

        if (c == '.') {
            if (i == 0) return true;
            char next = path.ptr[i - 1];
            if (zmm_p_is_separator(next)) return true;
            break;
        }
        if (i == 0) break;
    }

    return false;
}

// Helper to check if a slice ends with a specific character
static inline bool slice_ends_with_char(SliceCU8 s, char c) {
    if (s.len == 0) return false;
    return s.ptr[s.len - 1] == c;
}

// Helper to check if a slice starts with a specific character
static inline bool slice_starts_with_char(SliceCU8 s, char c) {
    if (s.len == 0) return false;
    return s.ptr[0] == c;
}
SliceU8 zmm_p_join_any(const SliceCU8* parts, u8* stack_buf,
                       usize stack_buf_size, u8** heap_out) {
    // 0. Initialize heap_out to NULL immediately
    if (heap_out) {
        *heap_out = NULL;
    }

    if (!parts) return NullSliceU8;

    // 1. Calculate total length
    usize total_len = 0;
    u32 last_non_empty_idx = U32_MAX;

    for (usize i = 0; parts[i].ptr != NULL; i++) {
        // Trim leading "./" right away as a pure view
        SliceCU8 part = zmm_p_trim_dot_slash(parts[i]);
        if (part.len == 0) continue;

        if (last_non_empty_idx != U32_MAX) {
            SliceCU8 prev = zmm_p_trim_dot_slash(parts[last_non_empty_idx]);
            // Only add separator if previous doesn't end in slash
            // and current doesn't start with a dot
            if (!slice_ends_with_char(prev, '/') &&
                !slice_starts_with_char(part, '.')) {
                total_len += 1;
            }
        }
        total_len += part.len;
        last_non_empty_idx = (u32)i;
    }

    if (total_len == 0) {
        return NullSliceU8;
    }

    // 2. Decide between stack buffer and heap allocation
    u8* buffer = NULL;
    if (total_len <= stack_buf_size && stack_buf != NULL) {
        buffer = stack_buf;
    } else {
        buffer = malloc(total_len);
        if (!buffer) return NullSliceU8;

        // Expose the heap pointer so the caller knows to free it
        if (heap_out) {
            *heap_out = buffer;
        }
    }

    // 3. Fill buffer
    usize offset = 0;
    last_non_empty_idx = U32_MAX;

    for (usize i = 0; parts[i].ptr != NULL; i++) {
        // Trim again for the copy phase
        SliceCU8 part = zmm_p_trim_dot_slash(parts[i]);
        if (part.len == 0) continue;

        if (last_non_empty_idx != U32_MAX) {
            SliceCU8 prev = zmm_p_trim_dot_slash(parts[last_non_empty_idx]);
            if (!slice_ends_with_char(prev, '/') &&
                !slice_starts_with_char(part, '.')) {
                buffer[offset] = '/';
                offset += 1;
            }
        }

        memcpy(buffer + offset, part.ptr, part.len);
        offset += part.len;
        last_non_empty_idx = (u32)i;
    }

    return (SliceU8){.ptr = buffer, .len = total_len};
}
SliceCU8 zmm_p_trim_dot_slash(SliceCU8 path) {
    if (!path.ptr || path.len == 0) return path;

    usize start = 0;
    while ((path.len - start) >= 2 && path.ptr[start] == '.' &&
           path.ptr[start + 1] == '/') {
        start += 2;
    }

    return (SliceCU8){.ptr = path.ptr + start, .len = path.len - start};
}
