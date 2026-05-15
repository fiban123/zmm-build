#pragma once

#include "export.h"
#include "num.h"
#include "slice.h"

/**
 * Returns the extension of `path` as a slice.
 * 
 * @param path The path to extract the extension from.
 * @return Slice containing the extension, or a nullslice if none.
 */
API SliceCU8 zmm_p_ext(SliceCU8 path);

/**
 * Returns the stem (filename without extension) of `path`.
 * 
 * @param path The path to extract the stem from.
 * @return Slice containing the stem.
 */
API SliceCU8 zmm_p_stem(SliceCU8 path);

/**
 * Returns the entire path with the extension removed.
 * 
 * @param path The path to strip the extension from.
 * @return Slice containing the path without extension.
 * 
 * [Note] Example: src/test/main.c -> src/test/main
 */
API SliceCU8 zmm_p_strip_ext(SliceCU8 path);

/**
 * Returns whether the path has the specified extension.
 * 
 * @param path The path to check.
 * @param ext The extension to look for.
 * @return true if the path has the extension, false otherwise.
 */
API bool zmm_p_has_ext(SliceCU8 path, SliceCU8 ext);

/**
 * Returns whether the path has any of the specified extensions.
 * 
 * @param path The path to check.
 * @param exts Nullslice-terminated array of extensions.
 * @return true if the path has any of the extensions, false otherwise.
 */
API bool zmm_p_has_exts(SliceCU8 path, const SliceCU8* exts);

/**
 * Returns whether the file is hidden (starts with a dot).
 * 
 * @param path The path to check.
 * @return true if it is hidden, false otherwise.
 */
API bool zmm_p_is_hidden(SliceCU8 path);

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
 * - It correctly handles directory delimeters and strips redundant "./" from paths.
 */
API SliceU8 zmm_p_join_any(const SliceCU8* parts, u8* stack_buf,
                           usize stack_buf_size, u8** heap_out);

/**
 * Trims leading "./" from a path.
 * 
 * @param path The path to trim.
 * @return Slice containing the trimmed path.
 */
SliceCU8 zmm_p_trim_dot_slash(SliceCU8 path);

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
inline static void zmm_p_normalize_sep(SliceU8 path) {
#ifdef _WIN32
    for (usize i = 0; i < path.len; i++) {
        if (path.ptr[i] == '\\') path.ptr[i] = '/';
    }
#else
    (void)path;
#endif
}
