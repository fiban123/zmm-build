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



API String zmm_p_join_arr(const StringView* parts) {
    // Pre-process: count parts, trim them, and decide which need a trailing '/'.
    //
    // Rules:
    //   - A part that is just "/" (standalone slash) is not emitted; instead it
    //     signals that the *previous* real part should be treated as a directory.
    //   - A part whose trimmed content ends in '/' is a directory (we strip the
    //     trailing slashes from it but remember to emit one separator).
    //   - Otherwise, the part is a filename — no separator after it.
    //   - Double slashes never appear.

    // 1. Count raw parts so we can stack-allocate metadata.
    usize n_raw = 0;
    for (usize i = 0; parts[i].ptr != NULL; i++) n_raw++;

    if (n_raw == 0) return (String){NULL, 0};

    // Trim each part once, stripping leading "./" and trailing '/'.
    // We store the cleaned view and a flag: "needs separator after".
    StringView* trimmed = (StringView*)malloc(n_raw * sizeof(StringView));
    bool* add_sep       = (bool*)malloc(n_raw * sizeof(bool));
    if (!trimmed || !add_sep) { free(trimmed); free(add_sep); return (String){NULL, 0}; }

    for (usize i = 0; i < n_raw; i++) {
        StringView t = zmm_p_trim_dot_slash(parts[i]);
        // Remember if the original trimmed part ended in '/' (directory marker).
        bool was_dir = (t.len > 0 && t.ptr[t.len - 1] == '/');
        // Strip all trailing slashes from the view.
        while (t.len > 0 && t.ptr[t.len - 1] == '/') t.len--;
        trimmed[i] = t;
        add_sep[i] = was_dir; // may be upgraded later by a standalone "/" part
    }

    // 2. Resolve standalone "/" parts: they mark the previous real part as a
    //    directory, then become empty so they won't be emitted.
    for (usize i = 0; i < n_raw; i++) {
        // A standalone "/" part has been fully stripped to len == 0,
        // but the original (after dot-slash trimming) was purely slashes.
        StringView orig = zmm_p_trim_dot_slash(parts[i]);
        bool is_standalone_slash = (orig.len > 0 && trimmed[i].len == 0);
        if (!is_standalone_slash) continue;

        // Mark the nearest preceding non-empty part as a directory.
        for (usize j = i; j > 0; j--) {
            if (trimmed[j - 1].len > 0) {
                add_sep[j - 1] = true;
                break;
            }
        }
        // This part contributes nothing.
    }

    // 3. Calculate total length (single O(n) pass).
    usize total_len = 0;
    usize last_real = SIZE_MAX;
    for (usize i = 0; i < n_raw; i++) {
        if (trimmed[i].len == 0) continue;
        if (last_real != SIZE_MAX && add_sep[last_real]) {
            total_len += 1; // separator between parts
        }
        total_len += trimmed[i].len;
        last_real = i;
    }
    // Trailing slash: if the last real part is a directory, include it.
    if (last_real != SIZE_MAX && add_sep[last_real]) {
        total_len += 1;
    }

    // 4. Allocate and fill.
    char* buffer = (char*)malloc(total_len);
    if (!buffer) { free(trimmed); free(add_sep); return (String){NULL, 0}; }

    usize offset = 0;
    last_real = SIZE_MAX;
    for (usize i = 0; i < n_raw; i++) {
        if (trimmed[i].len == 0) continue;
        if (last_real != SIZE_MAX && add_sep[last_real]) {
            buffer[offset++] = '/';
        }
        memcpy(buffer + offset, trimmed[i].ptr, trimmed[i].len);
        offset += trimmed[i].len;
        last_real = i;
    }
    // Emit trailing slash if the last part is a directory.
    if (last_real != SIZE_MAX && add_sep[last_real]) {
        buffer[offset++] = '/';
    }

    free(trimmed);
    free(add_sep);
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
