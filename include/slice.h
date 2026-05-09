#pragma once

#include <stdlib.h>
#include <string.h>

#include "num.h"
#include "stb/stb_ds.h"

/**
 * Declares a slice of type T.
 */
#define Slice(T)   \
    struct {       \
        T* ptr;    \
        usize len; \
    }

/**
 * Common slice types.
 */
typedef Slice(u8) SliceU8;
typedef Slice(const u8) SliceCU8;

#define NullSliceU8 (SliceU8){.ptr = nullptr, .len = 0}
#define NullSliceCU8 (SliceCU8){.ptr = nullptr, .len = 0}

/**
 * Creates a SliceCU8 from a string literal.
 */
#define strlit(S) (SliceCU8){.ptr = (const u8*)(S), .len = sizeof(S) - 1}

/**
 * Converts a Slice to a null-terminated C-string.
 * The resulting pointer MUST be freed with free().
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
 * Frees an stb_ds array of slices and each slice's ptr.
 */
#define slicearr_free(arr)                         \
    {                                              \
        for (usize i = 0; i < arrlenu(arr); i++) { \
            free(arr[i].ptr);                      \
        }                                          \
        arrfree(arr);                              \
    }
