/*
 * zmm-build: A compiled C build system
 * Copyright (C) 2026 @fiban123
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
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
