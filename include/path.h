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
#include "str.h"

API StringView zmm_p_ext(StringView path);

API StringView zmm_p_stem(StringView path);

API StringView zmm_p_strip_ext(StringView path);

API bool zmm_p_has_ext(StringView path, StringView ext);

API bool zmm_p_has_exts(StringView path, const StringView* exts);

API bool zmm_p_is_hidden(StringView path);

API String zmm_p_join_arr(const StringView* parts);

#define zmm_p_join(...) \
    zmm_p_join_arr((const StringView[]){__VA_ARGS__, {NULL, 0}})

StringView zmm_p_trim_dot_slash(StringView path);

UNUSED
static inline bool zmm_p_is_separator(char c) {
#ifdef _WIN32
    return c == '/' || c == '\\';
#else
    return c == '/';
#endif
}

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
