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

#pragma once

#include <stdio.h>

#include "str.h"
#include "vec.h"

typedef struct {
    FILE* f;
    vec(char) buf;
    usize count;
    void* lock;  // opaque pointer to pthread_mutex_t
} CompileCommands;

API int zmm_cc_init(CompileCommands* cc, StringView path);

API void zmm_cc_append(CompileCommands* cc, StringView dir, StringView file,
                       const char* args, usize num_args);

API void zmm_cc_finish(CompileCommands* cc);

typedef struct {
    StringView directory;
    StringView file;
    char* args;  // packed array of null-terminated strings
    usize num_args;
} CompileCommand;

API int zmm_cc_parse(CompileCommand** out, StringView path);
