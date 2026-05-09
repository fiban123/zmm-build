#include "export.h"
#include "num.h"
#include "slice.h"

/**
 * Returns the extension of `path` as a slice, or a nullslice if there is none.
 */
API SliceCU8 zmm_p_ext(SliceCU8 path);

/**
 * Returns the stem (filename without extension) of `path`.
 */
API SliceCU8 zmm_p_stem(SliceCU8 path);

/**
 * Returns the entire path with the extension removed.
 * e.g., "src/test/main.c" -> "src/test/main"
 */
API SliceCU8 zmm_p_strip_ext(SliceCU8 path);

/**
 * Returns whether the path has the specified extension.
 */
API bool zmm_p_has_ext(SliceCU8 path, SliceCU8 ext);

/**
 * Returns whether the path has any of the specified extensions.
 * exts must be a Nullslice-terminated array.
 */
API bool zmm_p_has_exts(SliceCU8 path, const SliceCU8* exts);

/**
 * Returns whether the file is hidden (starts with a dot).
 */
API bool zmm_p_is_hidden(SliceCU8 path);

/**
 * Joins multiple path parts together. it handles directories and strips
redundant
 * "./" from paths.
* - if a part ends in a `/`, it is assumed to be a directory, and a single slash
is kept
* - otherwise, the part is assumed to be a filename or part of a filename. no
separator is used.

 * NOTE: the result must be freed.

 * @param parts Nullslice-terminated array of path slices.
 * @param stack_buf Optional stack buffer to avoid allocation.
 * @param stack_buf_size Size of the stack buffer.
 * @param heap_out Pointer to store the heap-allocated buffer if used.
 * @return A slice pointing to the joined path.
 */
API SliceU8 zmm_p_join_any(const SliceCU8* parts, u8* stack_buf,
                           usize stack_buf_size, u8** heap_out);

/**
 * Trims leading "./" from a path.
 */
SliceCU8 zmm_p_trim_dot_slash(SliceCU8 path);

/**
 * Returns whether a character is a path separator.
 */
[[maybe_unused]]
static inline bool zmm_p_is_separator(char c) {
#ifdef _WIN32
    return c == '/' || c == '\\';
#else
    return c == '/';
#endif
}

/**
 * Normalizes all separators in a path to forward slashes.
 */
[[maybe_unused]]
inline static void zmm_p_normalize_sep(SliceU8 path) {
#ifdef _WIN32
    for (usize i = 0; i < path.len; i++) {
        if (path.ptr[i] == '\\') path.ptr[i] = '/';
    }
#else
    (void)path;
#endif
}
