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

#include <stdlib.h>
#include <string.h>

#include "num.h"

typedef struct {
    char* ptr;
    usize len;
} String;

typedef struct {
    const char* ptr;
    usize len;
} StringView;

static inline StringView zmm_str_ctov(const char* cstr) {
    return (StringView){cstr, strlen(cstr)};
}

// allocates a copy of cstr
static inline String zmm_str_ctos(const char* cstr) {
    String str;
    str.len = strlen(cstr);
    str.ptr = malloc(str.len);
    memcpy(str.ptr, cstr, str.len);
    return str;
}

static inline StringView zmm_str_stov(String str) {
    return (StringView){str.ptr, str.len};
}

static inline char* zmm_str_vtoc(StringView str) {
    char* cstr = malloc(str.len);
    memcpy(cstr, str.ptr, str.len);

    return cstr;
}

static inline bool zmm_str_eq(StringView a, StringView b) {
    if (a.len != b.len) return false;

    return memcmp(a.ptr, b.ptr, a.len) == 0;
}

#define strv(cstr) (StringView){cstr, sizeof(cstr) - 1}
