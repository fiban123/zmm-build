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
#include "num.h"
#include "str.h"

typedef struct {
    char* buf;
    usize buf_len;
    usize buf_cap;

    char** argv;
    usize argv_len;
    usize argv_cap;
} ArgvBuilder;

API int zmm_argv_initcap(ArgvBuilder* cmd, usize buf_cap, usize argv_cap);

API int zmm_argv_append(ArgvBuilder* argv, StringView arg);

API int zmm_argv_append_argv(ArgvBuilder* argv, const ArgvBuilder* src);

API int zmm_argv_pstart(ArgvBuilder* argv);

API int zmm_argv_pappend(ArgvBuilder* argv, StringView arg);

API int zmm_argv_pfinish(ArgvBuilder* argv);

void zmm_argv_free(ArgvBuilder* argv);

void zmm_argv_print(ArgvBuilder* argv);
