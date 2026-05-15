#pragma once

#include <stdlib.h>
#include <string.h>

#include "num.h"
#include "stb/stb_ds.h"

/**
 * Provides many useful macros for slices. These are mainly used
 * for length-based string handling. A Nullslice is any slice
 * where `.ptr` is `NULL`.
 */

/**
 * Declares a slice of type T.
 * A slice is a view into a contiguous array of elements.
 */
#define Slice(T)   \
    struct {       \
        T* ptr;    \
        usize len; \
    }

/**
 * Typedef for a slice containing `u8`. Used for mutable strings.
 */
typedef Slice(u8) SliceU8;

/**
 * Typedef for a slice containing `const u8`. Used for immutable strings.
 */
typedef Slice(const u8) SliceCU8;

/**
 * Macro for a nullslice of type SliceU8
 */
#define NullSliceU8 (SliceU8){.ptr = NULL, .len = 0}

/**
 * Macro for a nullslice of type SliceCU8
 */
#define NullSliceCU8 (SliceCU8){.ptr = NULL, .len = 0}

/**
 * Creates a SliceCU8 from a string literal.
 * 
 * [Note] This only works with string literals, not variables.
 */
#define strlit(S) (SliceCU8){.ptr = (const u8*)(S), .len = sizeof(S) - 1}

/**
 * Converts a Slice to a null-terminated C-string.
 * 
 * @param s The slice to convert.
 * @return A null-terminated C-string.
 * 
 * [FREE] The resulting pointer MUST be freed with free().
 */
#define slice_to_cstr(s)                       \
    ({                                         \
        __typeof__(s) _s = (s);                \
        char* _cstr = NULL;                    \
        if (_s.ptr != NULL) {                  \
            _cstr = (char*)malloc(_s.len + 1); \
            if (_cstr) {                       \
                memcpy(_cstr, _s.ptr, _s.len); \
                _cstr[_s.len] = '\0';          \
            }                                  \
        }                                      \
        _cstr;                                 \
    })

/**
 * Creates a slice literal from values.
 * 
 * @param Typedef The slice type (e.g., SliceCU8).
 * @param T The element type.
 * @param ... The values to include in the slice.
 */
#define slicelit(Typedef, T, ...)                     \
    (Typedef) {                                       \
        .ptr = (T[]){__VA_ARGS__},                    \
        .len = sizeof((T[]){__VA_ARGS__}) / sizeof(T) \
    }

/**
 * Creates an array of slices.
 */
#define slicearr(Typedef, ...) \
    (Typedef[]) { __VA_ARGS__ }

/**
 * Casts between slice types.
 */
#define slice_cast(ToType, s) \
    (ToType) { .ptr = (s).ptr, .len = (s).len }

/**
 * Returns whether two slices are equal.
 */
#define slice_eq(a, b)     \
    ((a).len == (b).len && \
     ((a.len) == 0 ||      \
      memcmp((a).ptr, (b).ptr, (a).len * sizeof(*(a).ptr)) == 0))

/**
 * Frees an stb_ds array of slices and each slice.
 * 
 * @param arr The stb_ds array to free.
 */
#define slicearr_free(arr)                         \
    {                                              \
        for (usize i = 0; i < arrlenu(arr); i++) { \
            free(arr[i].ptr);                      \
        }                                          \
        arrfree(arr);                              \
    }
