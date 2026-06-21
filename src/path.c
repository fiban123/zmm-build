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

#include "path.h"

#include "num.h"
#include "str.h"

// returns the extension of `path` as a slice, or a nullslice, if there is none
API StringView zmm_p_ext(StringView path) {
    if (path.len == 0) return (StringView){NULL, 0};
    for (usize i = path.len - 1;; i--) {
        char c = path.ptr[i];

        if (c == '.') {
            if (i == 0) break;
            char next = path.ptr[i - 1];
            if (!zmm_p_is_separator(next)) {
                return (StringView){.ptr = path.ptr + i,
                                    .len = path.len - 1 - i + 1};
            }
        }
        if (i == 0) break;
    }

    return (StringView){NULL, 0};
}

API StringView zmm_p_stem(StringView path) {
    if (path.len == 0) return (StringView){NULL, 0};
    ;

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
        return (StringView){NULL, 0};
    }

    // 2. Find the last dot in the basename to split the stem from the extension
    // We stop at `i > basename_start` so files like ".hidden" are treated as
    // the full stem rather than an empty stem with a ".hidden" extension.
    for (usize i = path.len - 1; i > basename_start; i--) {
        if (path.ptr[i] == '.') {
            return (StringView){.ptr = path.ptr + basename_start,
                                .len = i - basename_start};
        }
    }

    // 3. No valid extension found, so the entire basename is the stem
    return (StringView){.ptr = path.ptr + basename_start,
                        .len = path.len - basename_start};
}

API StringView zmm_p_strip_ext(StringView path) {
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
            return (StringView){.ptr = path.ptr, .len = i};
        }
    }

    // 3. No valid extension found, return the whole path
    return path;
}

API bool zmm_p_has_ext(StringView path, StringView target_ext) {
    StringView ext = zmm_p_ext(path);

    return zmm_str_eq(ext, target_ext);
}

API bool zmm_p_has_exts(StringView path, const StringView* exts) {
    StringView ext = zmm_p_ext(path);

    usize i = 0;

    StringView target_ext = exts[i];
    while (target_ext.ptr) {
        if (zmm_str_eq(ext, target_ext)) {
            return true;
        }
        i++;
        target_ext = exts[i];
    }

    return false;
}

API bool zmm_p_is_hidden(StringView path) {
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
static inline bool slice_ends_with_char(StringView s, char c) {
    if (s.len == 0) return false;
    return s.ptr[s.len - 1] == c;
}

// Helper to check if a slice starts with a specific character
static inline bool slice_starts_with_char(StringView s, char c) {
    if (s.len == 0) return false;
    return s.ptr[0] == c;
}

API String zmm_p_join_arr(const StringView* parts) {
    // 1. Calculate total length
    usize total_len = 0;
    u32 last_non_empty_idx = U32_MAX;

    for (usize i = 0; parts[i].ptr != NULL; i++) {
        // Trim leading "./" right away as a pure view
        StringView part = zmm_p_trim_dot_slash(parts[i]);
        if (part.len == 0) continue;

        if (last_non_empty_idx != U32_MAX) {
            StringView prev = zmm_p_trim_dot_slash(parts[last_non_empty_idx]);
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

    // 2. Always allocate on the heap
    char* buffer = malloc(total_len);
    if (!buffer) return (String){NULL, 0};

    // 3. Fill buffer
    usize offset = 0;
    last_non_empty_idx = U32_MAX;

    for (usize i = 0; parts[i].ptr != NULL; i++) {
        // Trim again for the copy phase
        StringView part = zmm_p_trim_dot_slash(parts[i]);
        if (part.len == 0) continue;

        if (last_non_empty_idx != U32_MAX) {
            StringView prev = zmm_p_trim_dot_slash(parts[last_non_empty_idx]);
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

    return (String){.ptr = buffer, .len = total_len};
}

API StringView zmm_p_trim_dot_slash(StringView path) {
    if (!path.ptr || path.len == 0) return path;

    usize start = 0;
    while ((path.len - start) >= 2 && path.ptr[start] == '.' &&
           path.ptr[start + 1] == '/') {
        start += 2;
    }

    return (StringView){.ptr = path.ptr + start, .len = path.len - start};
}
