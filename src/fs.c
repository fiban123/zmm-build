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

#include "fs.h"

#include <dirent.h>
#include <errno.h>
#include <stb/stb_ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define chdir _chdir
#endif
#include <unistd.h>

#include "path.h"
#include "slice.h"

static char* path_join(const char* dir, const char* file) {
    usize dlen = strlen(dir);
    usize flen = strlen(file);
    char* out = malloc(dlen + flen + 2);

    if (dlen > 0 && dir[dlen - 1] != '/' && dir[dlen - 1] != '\\') {
        sprintf(out, "%s/%s", dir, file);
    } else {
        sprintf(out, "%s%s", dir, file);
    }
    return out;
}

static inline bool matches_kinds(FindKind kind, const FindKind* targets) {
    if (!targets || targets[0] == 0) return true;
    for (usize i = 0; targets[i] != 0; i++) {
        if (kind == targets[i]) return true;
    }
    return false;
}

static i32 find_append_recursive(arr(SliceU8) * out, const char* current_path,
                                 FindOpts opts) {
    DIR* dir = opendir(current_path);
    SliceCU8 cur_slice_c = {.ptr = (const u8*)current_path,
                            .len = strlen(current_path)};

    if (!dir) {
        // Bonus Feature: The root is actually a specific file!
        struct stat st;
        if (stat(current_path, &st) == 0 && S_ISREG(st.st_mode)) {
            if (!matches_kinds(FK_FILE, opts.kinds)) return 0;
            if (opts.ignore_hidden && zmm_p_is_hidden(cur_slice_c)) return 0;
            if (opts.exts && opts.exts[0].ptr != NULL &&
                !zmm_p_has_exts(cur_slice_c, opts.exts))
                return 0;

            char* trimmed_root = strdup(current_path);
            SliceU8 slice = {.ptr = (u8*)trimmed_root,
                             .len = strlen(trimmed_root)};
            zmm_p_normalize_sep(slice);
            arrpush(*out, slice);
        }
        return 0;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        SliceCU8 entry_slice_c = {.ptr = (const u8*)entry->d_name,
                                  .len = strlen(entry->d_name)};

        if (opts.ignore_hidden && zmm_p_is_hidden(entry_slice_c)) {
            continue;
        }

        char* full_path = path_join(current_path, entry->d_name);
        SliceU8 full_slice = {.ptr = (u8*)full_path, .len = strlen(full_path)};
        zmm_p_normalize_sep(full_slice);

        // Determine Kind using universally supported `stat`
        FindKind kind = FK_FILE;
        struct stat st;
        if (stat(full_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                kind = FK_DIRECTORY;
            }
        }

        bool match = true;
        if (!matches_kinds(kind, opts.kinds)) match = false;

        // Extension filtering logic
        if (opts.exts && opts.exts[0].ptr != NULL) {
            if (kind == FK_DIRECTORY) {
                // Directories automatically fail the match if specific
                // extensions are requested
                match = false;
            } else if (!zmm_p_has_exts(entry_slice_c, opts.exts)) {
                match = false;
            }
        }

        if (match) {
            char* saved_path = strdup(full_path);
            SliceU8 slice = {.ptr = (u8*)saved_path, .len = strlen(saved_path)};
            arrpush(*out, slice);
        }

        // Because of the early exit above, hidden directories will never reach
        // this point
        if (opts.depth == FD_RECURSIVE && kind == FK_DIRECTORY) {
            find_append_recursive(out, full_path, opts);
        }

        free(full_path);
    }

    closedir(dir);
    return 0;
}

i32 zmm_fs_find(arr(SliceU8) * out, const SliceCU8* roots, FindOpts opts) {
    if (!out || !roots) return -1;

    for (usize i = 0; roots[i].ptr != NULL; i++) {
        SliceCU8 root = roots[i];

        char* null_root = malloc(root.len + 1);
        memcpy(null_root, root.ptr, root.len);
        null_root[root.len] = '\0';

        find_append_recursive(out, null_root, opts);

        free(null_root);
    }

    return 0;
}

// Equivalent to `mkdir -p`
i32 zmm_fs_create_dir_path(SliceCU8 path) {
    char* tmp = slice_to_cstr(path);
    if (!tmp) return -1;

    // Normalize to forward slashes for the iteration logic
    for (char* p = tmp; *p; p++) {
        if (*p == '\\') *p = '/';
    }

    char* p = tmp;
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';  // Temporarily terminate
#if defined(_WIN32)
            mkdir(tmp);
#else
            mkdir(tmp, 0755);
#endif
            *p = '/';  // Restore slash
        }
    }

    // Create the final leaf directory
#if defined(_WIN32)
    i32 res = (mkdir(tmp) != 0 && errno != EEXIST) ? -1 : 0;
#else
    i32 res = (mkdir(tmp, 0755) != 0 && errno != EEXIST) ? -1 : 0;
#endif

    free(tmp);
    return res;
}

i32 zmm_fs_create_parent_path(SliceCU8 path) {
    char* tmp = slice_to_cstr(path);
    if (!tmp) return -1;

    // Find the last separator to slice off the file name
    char* last_slash = NULL;
    for (char* p = tmp; *p; p++) {
        if (*p == '/' || *p == '\\') {
            last_slash = p;
        }
    }

    i32 res = 0;
    if (last_slash) {
        *last_slash = '\0';  // Truncate at the last slash
        SliceCU8 dir_slice = {.ptr = (const u8*)tmp, .len = strlen(tmp)};
        res = zmm_fs_create_dir_path(dir_slice);
    }

    free(tmp);
    return res;
}

bool zmm_fs_file_exists(SliceCU8 path) {
    char* cstr = slice_to_cstr(path);
    if (!cstr) return false;

#if defined(_WIN32)
    bool exists = (access(cstr, 0) == 0);
#else
    bool exists = (access(cstr, F_OK) == 0);
#endif

    free(cstr);
    return exists;
}

i32 zmm_fs_delete_file(SliceCU8 path) {
    char* cstr = slice_to_cstr(path);
    if (!cstr) return -1;

    i32 res = 0;
    if (remove(cstr) != 0 && errno != ENOENT) {
        res = -1;
    }

    free(cstr);
    return res;
}

i32 zmm_fs_delete_dir(SliceCU8 path) {
    char* cstr = slice_to_cstr(path);
    if (!cstr) return -1;

    i32 res = 0;
    if (rmdir(cstr) != 0 && errno != ENOENT) {
        res = -1;
    }

    free(cstr);
    return res;
}

static i32 delete_tree_cstr(const char* path) {
    DIR* dir = opendir(path);
    if (!dir) {
        // If we can't open it as a directory, try deleting it as a file
        return (remove(path) != 0 && errno != ENOENT) ? -1 : 0;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char* full_path = path_join(path, entry->d_name);
        delete_tree_cstr(full_path);  // Recurse
        free(full_path);
    }
    closedir(dir);

    // Now that the directory is empty, delete the directory itself
    return (rmdir(path) != 0 && errno != ENOENT) ? -1 : 0;
}

i32 zmm_fs_delete_tree(SliceCU8 path) {
    char* cstr = slice_to_cstr(path);
    if (!cstr) return -1;

    i32 res = delete_tree_cstr(cstr);

    free(cstr);
    return res;
}

SliceU8 zmm_fs_abs_cwd(void) {
    char buf[4096];
    if (getcwd(buf, sizeof(buf)) != NULL) {
        char* heap_path = strdup(buf);
        return (SliceU8){.ptr = (u8*)heap_path, .len = strlen(heap_path)};
    }
    return NullSliceU8;
}

i32 zmm_fs_change_cwd(SliceCU8 path) {
    char* path_nul = slice_to_cstr(path);
    if (!path_nul) return -1;

    if (chdir(path_nul) == 0) {
        free(path_nul);
        return 0;
    } else {
        free(path_nul);
        return -1;
    }
}
