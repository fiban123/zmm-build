/*
 * zmm-build: A compiled C build system
 * Copyright 2026 fiban123
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
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
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define chdir _chdir
#include <io.h>
#endif
#include <unistd.h>

#include "path.h"
#include "print.h"
#include "vec.h"

static bool fs_dry_delete = false;

API void zmm_fs_set_dry_delete(bool enable) { fs_dry_delete = enable; }

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

static int find_append_recursive(vec(String) * out, const char* current_path,
                                 FindOpts opts) {
    DIR* dir = opendir(current_path);
    StringView cur_sv = zmm_str_ctov(current_path);

    if (!dir) {
        // Bonus Feature: The root is actually a specific file!
        struct stat st;
        if (stat(current_path, &st) == 0 && S_ISREG(st.st_mode)) {
            if (opts.kind != FK_FILE) return 0;
            if (opts.ignore_hidden && zmm_p_is_hidden(cur_sv)) return 0;

            // Extension filtering logic
            if (opts.ext.ptr != NULL && opts.ext.len > 0) {
                if (!zmm_p_has_ext(cur_sv, opts.ext)) return 0;
            }

            char* trimmed_root = strdup(current_path);
            String str = {trimmed_root, strlen(trimmed_root)};
            zmm_p_normalize_sep(str);
            vecpush(*out, str);
        }
        return 0;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        StringView entry_sv = zmm_str_ctov(entry->d_name);

        if (opts.ignore_hidden && zmm_p_is_hidden(entry_sv)) {
            continue;
        }

        char* full_path = path_join(current_path, entry->d_name);
        String full_str = {full_path, strlen(full_path)};
        zmm_p_normalize_sep(full_str);

        // Determine Kind using universally supported `stat`
        FindKind kind = FK_FILE;
        struct stat st;
        if (stat(full_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                kind = FK_DIRECTORY;
            }
        }

        bool match = true;
        if (kind != opts.kind) match = false;

        // Extension filtering logic
        if (opts.ext.ptr != NULL && opts.ext.len > 0) {
            if (kind == FK_DIRECTORY) {
                // Directories automatically fail the match if specific
                // extensions are requested
                match = false;
            } else if (!zmm_p_has_ext(entry_sv, opts.ext)) {
                match = false;
            }
        }

        if (match) {
            char* saved_path = strdup(full_path);
            String str = {saved_path, strlen(saved_path)};
            vecpush(*out, str);
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

API int zmm_fs_find(vec(String) * out, StringView root, FindOpts opts) {
    char* null_root = zmm_str_vtoc(root);
    find_append_recursive(out, null_root, opts);
    free(null_root);

    return 0;
}

// Equivalent to `mkdir -p`
API int zmm_fs_create_dir_path(StringView path) {
    char* tmp = zmm_str_vtoc(path);
    if (!tmp) return 1;

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
    int res = (mkdir(tmp) != 0 && errno != EEXIST) ? 1 : 0;
#else
    int res = (mkdir(tmp, 0755) != 0 && errno != EEXIST) ? 1 : 0;
#endif

    free(tmp);
    return res;
}

API int zmm_fs_create_parent_path(StringView path) {
    char* tmp = zmm_str_vtoc(path);
    if (!tmp) return 1;

    // Find the last separator to slice off the file name
    char* last_slash = NULL;
    for (char* p = tmp; *p; p++) {
        if (*p == '/' || *p == '\\') {
            last_slash = p;
        }
    }

    int res = 0;
    if (last_slash) {
        *last_slash = '\0';  // Truncate at the last slash
        StringView dir_slice = zmm_str_ctov(tmp);
        res = zmm_fs_create_dir_path(dir_slice);
    }

    free(tmp);
    return res;
}

API bool zmm_fs_file_exists(StringView path) {
    char* cstr = zmm_str_vtoc(path);
    if (!cstr) return false;

#if defined(_WIN32)
    bool exists = (access(cstr, 0) == 0);
#else
    bool exists = (access(cstr, F_OK) == 0);
#endif

    free(cstr);
    return exists;
}

API int zmm_fs_delete_file(StringView path) {
    if (fs_dry_delete) {
        zmm_lprintf("Pretending to delete file %s\n", path);
        zmm_lemit();
        return 0;
    }
    zmm_lprintf("Deleting file %s\n", path);
    zmm_lemit();

    char* cstr = zmm_str_vtoc(path);
    if (!cstr) return 1;

    int res = 0;
    if (remove(cstr) != 0 && errno != ENOENT) {
        res = 1;
    }

    free(cstr);
    return res;
}

API int zmm_fs_delete_dir(StringView path) {
    if (fs_dry_delete) {
        zmm_lprintf("Pretending to delete directory %s\n", path);
        zmm_lemit();
        return 0;
    }
    zmm_lprintf("Deleting directory %s\n", path);
    zmm_lemit();

    char* cstr = zmm_str_vtoc(path);
    if (!cstr) return 1;

    int res = 0;
    if (rmdir(cstr) != 0 && errno != ENOENT) {
        res = 1;
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
    return (rmdir(path) != 0 && errno != ENOENT) ? 1 : 0;
}

API int zmm_fs_delete_tree(StringView path) {
    if (fs_dry_delete) {
        zmm_lprintf("Pretending to delete tree %s\n", path);
        zmm_lemit();
        return 0;
    }
    zmm_lprintf("Deleting tree %s\n", path);
    zmm_lemit();

    char* cstr = zmm_str_vtoc(path);
    if (!cstr) return 1;

    int res = delete_tree_cstr(cstr);

    free(cstr);
    return res;
}

static int copy_file_cstr(const char* src, const char* dst) {
    FILE* in = fopen(src, "rb");
    if (!in) return 1;

    FILE* out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return 1;
    }

    char buf[BUFSIZ];
    size_t n;
    int res = 0;

    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            res = -1;  // Write error
            break;
        }
    }

    fclose(in);
    fclose(out);

    // If the copy was successful, copy the metadata (permissions)
    if (res == 0) {
        struct stat st;
        if (stat(src, &st) == 0) {
#if defined(_WIN32)
            // Windows permissions are simpler and executability depends on the
            // .exe extension, but we copy the read/write mode anyway.
            _chmod(dst, st.st_mode);
#else
            // On POSIX (Linux/macOS), this ensures +x (executable) and other
            // permissions are correctly applied.
            // 07777 grabs the setuid/setgid/sticky bits + standard rwx bits.
            chmod(dst, st.st_mode & 07777);
#endif
        } else {
            // If stat fails, we still copied the data, but metadata failed
            res = 1;
        }
    }

    return res;
}

static int copy_dir_cstr(const char* src, const char* dst) {
    // Ensure destination directory exists
    StringView dst_slice = zmm_str_ctov(dst);
    zmm_fs_create_dir_path(dst_slice);

    DIR* dir = opendir(src);
    if (!dir) return 1;

    struct dirent* entry;
    int res = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char* src_path = path_join(src, entry->d_name);
        char* dst_path = path_join(dst, entry->d_name);

        struct stat st;
        // Shallow copy: only copy regular files
        if (stat(src_path, &st) == 0 && S_ISREG(st.st_mode)) {
            if (copy_file_cstr(src_path, dst_path) != 0) {
                res = 1;
            }
        }

        free(src_path);
        free(dst_path);
    }

    closedir(dir);
    return res;
}

static int copy_tree_cstr(const char* src, const char* dst) {
    struct stat st;
    if (stat(src, &st) != 0) return 1;

    if (S_ISREG(st.st_mode)) {
        return copy_file_cstr(src, dst);
    } else if (S_ISDIR(st.st_mode)) {
        // Ensure destination directory exists
        StringView dst_slice = zmm_str_ctov(dst);
        zmm_fs_create_dir_path(dst_slice);

        DIR* dir = opendir(src);
        if (!dir) return 1;

        struct dirent* entry;
        int res = 0;

        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 ||
                strcmp(entry->d_name, "..") == 0)
                continue;

            char* src_path = path_join(src, entry->d_name);
            char* dst_path = path_join(dst, entry->d_name);

            // Recurse into files and directories
            if (copy_tree_cstr(src_path, dst_path) != 0) {
                res = 1;
            }

            free(src_path);
            free(dst_path);
        }

        closedir(dir);
        return res;
    }

    return 1;  // Not a regular file or directory
}

// --- Public API ---

API int zmm_fs_copy_file(StringView src, StringView dst) {
    zmm_lprintf("Copying file %s to %s\n", src, dst);
    zmm_lemit();

    char* src_cstr = zmm_str_vtoc(src);
    char* dst_cstr = zmm_str_vtoc(dst);

    if (!src_cstr || !dst_cstr) {
        free(src_cstr);
        free(dst_cstr);
        return 1;
    }

    int res = copy_file_cstr(src_cstr, dst_cstr);

    free(src_cstr);
    free(dst_cstr);
    return res;
}

API int zmm_fs_copy_dir(StringView src, StringView dst) {
    zmm_lprintf("Copying directory %s to %s\n", src, dst);
    zmm_lemit();

    char* src_cstr = zmm_str_vtoc(src);
    char* dst_cstr = zmm_str_vtoc(dst);

    if (!src_cstr || !dst_cstr) {
        free(src_cstr);
        free(dst_cstr);
        return 1;
    }

    int res = copy_dir_cstr(src_cstr, dst_cstr);

    free(src_cstr);
    free(dst_cstr);
    return res;
}

API int zmm_fs_copy_tree(StringView src, StringView dst) {
    zmm_lprintf("Copying tree %s to %s\n", src, dst);
    zmm_lemit();

    char* src_cstr = zmm_str_vtoc(src);
    char* dst_cstr = zmm_str_vtoc(dst);

    if (!src_cstr || !dst_cstr) {
        free(src_cstr);
        free(dst_cstr);
        return 1;
    }

    i32 res = copy_tree_cstr(src_cstr, dst_cstr);

    free(src_cstr);
    free(dst_cstr);
    return res;
}

API String zmm_fs_abs_cwd(void) {
    char buf[4096];
    if (getcwd(buf, sizeof(buf)) != NULL) {
        return zmm_str_ctos(buf);
    }
    return (String){NULL, 0};
}

API int zmm_fs_change_cwd(StringView path) {
    char* path_nul = zmm_str_vtoc(path);
    if (!path_nul) return 1;

    if (chdir(path_nul) == 0) {
        free(path_nul);
        return 0;
    } else {
        free(path_nul);
        return 1;
    }
}
