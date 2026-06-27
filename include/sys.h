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

typedef enum {
    TERM_EXITED,    // The process exited normally with a return code.
    TERM_SIGNALED,  // The process was forcibly terminated by the OS.
    TERM_ERROR      // The process failed to even start.
} TermType;

typedef struct {
    int code;  // 0 if success
    TermType type;
} ChildTerm;

static inline int zmm_sys_term_code(ChildTerm t) {
    return t.type == TERM_EXITED ? t.code : 1;
}

typedef struct {
    ChildTerm status;
    String output;
} ExecResult;

API ExecResult zmm_sys_exec(char* const* argv, usize num_args);

API ChildTerm zmm_sys_exec_print(char* const* argv, usize num_args);

API ChildTerm zmm_sys_exec_redirect(char* const* argv, usize num_args);

API ExecResult zmm_sys_exec_flat(const char* arg_buf, usize num_args);

API ChildTerm zmm_sys_exec_print_flat(const char* arg_buf, usize num_args);

API ChildTerm zmm_sys_exec_redirect_flat(const char* arg_buf, usize num_args);

API void zmm_sys_set_verbose(bool verbose);
API bool zmm_sys_get_verbose(void);

