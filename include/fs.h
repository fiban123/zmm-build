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

#include <stdint.h>

#include "export.h"
#include "num.h"
#include "str.h"
#include "vec.h"

typedef enum : u8 { FD_SHALLOW, FD_RECURSIVE } FindDepth;

typedef enum : u8 { FK_FILE, FK_DIRECTORY } FindKind;

typedef struct {
    FindDepth depth;
    FindKind kind;
    StringView ext;
    bool ignore_hidden;
} FindOpts;

API int zmm_fs_find(vec(String) * out, StringView root, FindOpts opts);

API int zmm_fs_create_dir_path(StringView path);

API int zmm_fs_create_parent_path(StringView path);

API bool zmm_fs_file_exists(StringView path);

API int zmm_fs_delete_file(StringView path);

API int zmm_fs_delete_dir(StringView path);

API int zmm_fs_delete_tree(StringView path);

API int zmm_fs_copy_file(StringView src, StringView dst);

API int zmm_fs_copy_dir(StringView src, StringView dst);

API int zmm_fs_copy_tree(StringView src, StringView dst);

API String zmm_fs_abs_cwd();

API int zmm_fs_change_cwd(StringView path);
