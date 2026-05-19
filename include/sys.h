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
#include "slice.h"

/**
 * Enum of ways a child process can terminate.
 */
typedef enum {
    TERM_EXITED,    // The process exited normally with a return code.
    TERM_SIGNALED,  // The process was forcibly terminated by the OS.
    TERM_ERROR      // The process failed to even start.
} TermType;

/**
 * Information about a terminated child process.
 */
typedef struct {
    TermType type;  // How the child process terminated
    i32 code;       // The exit code of the process (only valid if type is TERM_EXITED).
} ChildTerm;

/**
 * Bundles the execution status with the captured output buffer.
 */
typedef struct {
    ChildTerm status;
    SliceU8 output;
} ExecResult;

/**
 * Executes a command and captures its output.
 * It merges stderr into stdout.
 *
 * @param argv Null-terminated array of argument strings.
 * @param num_args Number of arguments.
 * @return ExecResult containing the output and status.
 *
 * [FREE] The caller must free the output on the returned ExecResult.
 */
API ExecResult zmm_sys_exec(char* const* argv, usize num_args);

/**
 * Executes a command and prints its output thread-safely.
 *
 * @param argv Null-terminated array of argument strings.
 * @param num_args Number of arguments.
 * @return The execution status.
 */
API ChildTerm zmm_sys_exec_print(char* const* argv, usize num_args);

/**
 * Executes a command without capturing output.
 * The child process inherits stdout/stderr from the parent.
 *
 * @param argv Null-terminated array of argument strings.
 * @param num_args Number of arguments.
 * @return The execution status.
 */
API ChildTerm zmm_sys_exec_redirect(char* const* argv, usize num_args);

/**
 * Executes a command from a packed argument buffer and captures output.
 *
 * @param arg_buf Packed array of null-terminated strings.
 * @param num_args Number of arguments.
 * @return ExecResult containing the output and status.
 *
 * [FREE] The caller must free the output on the returned ExecResult.
 */
API ExecResult zmm_sys_exec_flat(const char* arg_buf, usize num_args);

/**
 * Executes a command from a packed argument buffer and prints output.
 *
 * @param arg_buf Packed array of null-terminated strings.
 * @param num_args Number of arguments.
 * @return The execution status.
 */
API ChildTerm zmm_sys_exec_print_flat(const char* arg_buf, usize num_args);

/**
 * Executes a command from a packed argument buffer without capturing output.
 *
 * @param arg_buf Packed array of null-terminated strings.
 * @param num_args Number of arguments.
 * @return The execution status.
 */
API ChildTerm zmm_sys_exec_redirect_flat(const char* arg_buf, usize num_args);
