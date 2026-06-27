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
    char** argv;
    usize argv_len;
    usize argv_cap;

    char* buf;
    usize buf_len;
    usize buf_cap;
} ArgvBuilder;

API int zmm_argvb_initcap(ArgvBuilder* cmd, usize buf_cap, usize argv_cap);

API int zmm_argvb_append(ArgvBuilder* argvb, StringView arg);

API int zmm_argvb_appendc(ArgvBuilder* argvb, const char* arg);

API int zmm_argvb_append_n(ArgvBuilder* argvb, const StringView* args, usize n);

API int zmm_argvb_appendc_n(ArgvBuilder* argvb, const char* const* args,
                            usize n);

API int zmm_argvb_append_argv(ArgvBuilder* argvb, char* const* argv,
                              usize argv_len);

API int zmm_argvb_pstart(ArgvBuilder* argvb);

API int zmm_argvb_pappend(ArgvBuilder* argvb, StringView arg);

API int zmm_argvb_pappend_i64(ArgvBuilder* argvb, i64 val);

API int zmm_argvb_pappend_u64(ArgvBuilder* argvb, u64 val);

API int zmm_argvb_pappend_float(ArgvBuilder* argvb, float val);

API int zmm_argvb_pappend_double(ArgvBuilder* argvb, double val);

API int zmm_argvb_pfinish(ArgvBuilder* argvb);

API void zmm_argvb_free(ArgvBuilder* argvb);

void zmm_argv_print(char* const* argv, usize argv_len);
