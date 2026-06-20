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

#include "export.h"
#include "num.h"
#include "str.h"

/**
 * Returns the extension of `path` as a slice.
 *
 * @param path The path to extract the extension from.
 * @return Slice containing the extension, or a nullslice if none.
 */
API StringView zmm_p_ext(StringView path);

/**
 * Returns the stem (filename without extension) of `path`.
 *
 * @param path The path to extract the stem from.
 * @return Slice containing the stem.
 */
API StringView zmm_p_stem(StringView path);

/**
 * Returns the entire path with the extension removed.
 *
 * @param path The path to strip the extension from.
 * @return Slice containing the path without extension.
 *
 * [Note] Example: src/test/main.c -> src/test/main
 */
API StringView zmm_p_strip_ext(StringView path);

/**
 * Returns whether the path has the specified extension.
 *
 * @param path The path to check.
 * @param ext The extension to look for.
 * @return true if the path has the extension, false otherwise.
 */
API bool zmm_p_has_ext(StringView path, StringView ext);

/**
 * Returns whether the path has any of the specified extensions.
 *
 * @param path The path to check.
 * @param exts Nullslice-terminated array of extensions.
 * @return true if the path has any of the extensions, false otherwise.
 */
API bool zmm_p_has_exts(StringView path, const StringView* exts);

/**
 * Returns whether the file is hidden (starts with a dot).
 *
 * @param path The path to check.
 * @return true if it is hidden, false otherwise.
 */
API bool zmm_p_is_hidden(StringView path);

/**
 * Joins multiple path parts together into a single path.
 *
 * @param parts Nullslice-terminated array of path parts.
 * @param stack_buf Optional stack buffer to avoid allocation.
 * @param stack_buf_size Size of the stack buffer.
 * @param heap_out Pointer to store the heap-allocated buffer if used.
 * @return A slice pointing to the joined path.
 *
 * [FREE] The resulting *heap_out must be freed if it is not NULL.
 * [Note]
 * - if a part ends with a `/`, it is interpreted as a directory
 * - Otherwise it is interpreted as a file
 * - It correctly handles directory delimeters and strips redundant "./" from
 * paths.
 */
API String zmm_p_join_any(const StringView* parts, u8* stack_buf,
                          usize stack_buf_size, u8** heap_out);

/**
 * Trims leading "./" from a path.
 *
 * @param path The path to trim.
 * @return Slice containing the trimmed path.
 */
StringView zmm_p_trim_dot_slash(StringView path);

/**
 * Returns whether a character is a path separator.
 *
 * @param c The character to check.
 * @return true if it is a separator, false otherwise.
 */
UNUSED
static inline bool zmm_p_is_separator(char c) {
#ifdef _WIN32
    return c == '/' || c == '\\';
#else
    return c == '/';
#endif
}

/**
 * Normalizes all separators in a path to forward slashes.
 *
 * @param path The path to normalize (modified in-place).
 */
UNUSED
inline static void zmm_p_normalize_sep(String path) {
#ifdef _WIN32
    for (usize i = 0; i < path.len; i++) {
        if (path.ptr[i] == '\\') path.ptr[i] = '/';
    }
#else
    (void)path;
#endif
}
