#pragma once

#include <stdint.h>

#include "arr.h"
#include "export.h"
#include "num.h"
#include "slice.h"

typedef enum : u8 { FD_SHALLOW, FD_RECURSIVE } FindDepth;

typedef enum : u8 { FK_FILE = 1, FK_DIRECTORY } FindKind;

typedef struct {
    FindDepth depth;

    const FindKind* kinds;  // null-terminated

    const SliceCU8* exts;  // null-terminated

    bool ignore_hidden;
} FindOpts;

/** 
 * Finds all matches given the roots and find options, appends them to out.
 * @param out Pointer to an stb_ds array of SliceU8.
 * @param roots Null-terminated array of root directories to search in.
 * @param opts Search options (depth, kinds, extensions, hidden).
 * @return 0 on success, otherwise an error code.
 * NOTE: The result must be freed with slicearr_free().
 **/
API i32 zmm_fs_find(arr(SliceU8) * out, const SliceCU8* roots, FindOpts opts);

/**
 * Creates a directory and all its parents (equivalent to `mkdir -p`).
 */
API i32 zmm_fs_create_dir_path(SliceCU8 path);

/**
 * Creates the parent directory of a file path.
 */
API i32 zmm_fs_create_parent_path(SliceCU8 path);

/**
 * Returns whether a file or directory exists.
 */
API bool zmm_fs_file_exists(SliceCU8 path);

/**
 * Deletes a file.
 */
API i32 zmm_fs_delete_file(SliceCU8 path);

/**
 * Deletes an empty directory.
 */
API i32 zmm_fs_delete_dir(SliceCU8 path);

/**
 * Recursively deletes a directory tree.
 */
API i32 zmm_fs_delete_tree(SliceCU8 path);

/**
 * Returns the absolute path of the current working directory.
 * NOTE: The resulting slice's ptr must be freed.
 */
API SliceU8 zmm_fs_abs_cwd(void);

/**
 * Changes the current working directory.
 */
API i32 zmm_fs_change_cwd(SliceCU8 path);
