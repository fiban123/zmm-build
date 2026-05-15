#pragma once

#include <stdint.h>

#include "arr.h"
#include "export.h"
#include "num.h"
#include "slice.h"

/**
 * Specifies the depth of a filesystem search.
 */
typedef enum : u8 {
    FD_SHALLOW,   // Only search the specified directories.
    FD_RECURSIVE  // Search specified directories and all subdirectories.
} FindDepth;

/**
 * Specifies the kind of filesystem entry to find.
 * 
 * NOTE: more filsystem entry types such as symlinks exist and may be added in a future version.
 */
typedef enum : u8 {
    FK_FILE = 1,  // Find files.
    FK_DIRECTORY  // Find directories.
} FindKind;

/**
 * Options for configuring a filesystem search via zmm_fs_find.
 */
typedef struct {
    FindDepth depth;
    const FindKind* kinds;  // Null-terminated array of FindKind.
    const SliceCU8* exts;   // Nullslice-terminated array of extensions.
    bool ignore_hidden;     // If true, ignore files starting with a dot.
} FindOpts;

/**
 * Finds all matches given the roots and find options, appends them to out.
 * 
 * @param out Pointer to an stb_ds array of SliceU8.
 * @param roots Nullslice-terminated array of root directories to search in.
 * @param opts Search options (depth, kinds, extensions, hidden).
 * @return 0 on success, otherwise -1.
 * 
 * [FREE] The results appended to 'out' must be freed with slicearr_free().
 */
API i32 zmm_fs_find(arr(SliceU8) * out, const SliceCU8* roots, FindOpts opts);

/**
 * Creates a directory and all its parents (equivalent to `mkdir -p`).
 * 
 * @param path The directory path to create.
 * @return 0 on success, otherwise -1.
 */
API i32 zmm_fs_create_dir_path(SliceCU8 path);

/**
 * Creates the parent directory of a file path.
 * 
 * @param path The file path whose parent directory should be created.
 * @return 0 on success, otherwise -1.
 */
API i32 zmm_fs_create_parent_path(SliceCU8 path);

/**
 * Returns whether a file or directory exists.
 * 
 * @param path The path to check.
 * @return true if it exists, false otherwise.
 */
API bool zmm_fs_file_exists(SliceCU8 path);

/**
 * Deletes a file.
 * 
 * @param path The path to the file to delete.
 * @return 0 on success, otherwise -1.
 */
API i32 zmm_fs_delete_file(SliceCU8 path);

/**
 * Deletes an empty directory.
 * 
 * @param path The path to the directory to delete.
 * @return 0 on success, otherwise -1.
 */
API i32 zmm_fs_delete_dir(SliceCU8 path);

/**
 * Recursively deletes a directory tree.
 * 
 * @param path The path to the directory tree to delete.
 * @return 0 on success, otherwise -1.
 */
API i32 zmm_fs_delete_tree(SliceCU8 path);

/**
 * Returns the absolute path of the current working directory.
 * 
 * @return Slice containing the absolute CWD path or a nullslice on a failure.
 * 
 * [FREE] The resulting slice's ptr must be freed.
 */
API SliceU8 zmm_fs_abs_cwd(void);

/**
 * Changes the current working directory.
 * 
 * @param path The new working directory path.
 * @return 0 on success, otherwise -1.
 */
API i32 zmm_fs_change_cwd(SliceCU8 path);
