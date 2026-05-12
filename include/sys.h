#pragma once

#include "export.h"
#include "num.h"
#include "slice.h"

typedef enum {
    TERM_EXITED,    // returned an exit code
    TERM_SIGNALED,  // forcibly terminated by OS
    TERM_ERROR      // failed to even start the process
} TermType;

typedef struct {
    TermType type;
    i32 code;  // only valid if TermType is TERM_EXITED
} ChildTerm;

typedef enum {
    EXEC_SUCCESS = 0,
    EXEC_ERR_PIPE,
    EXEC_ERR_FORK,
    EXEC_ERR_CREATE_PROCESS,
    EXEC_ERR_OOM
} ExecError;

// struct representing the state of the execution
typedef struct {
    ChildTerm term;
    ExecError err;
} ExecStatus;

// Bundles the execution status with the allocated output buffer
typedef struct {
    ExecStatus status;
    SliceU8 output;
} ExecResult;

/**
 * Frees the output buffer allocated by zmm_sys_exec.
 */
API void zmm_sys_exec_result_free(ExecResult* res);

/**
 * Executes a command, merging stderr into stdout at the OS level.
 * @param arg_buf Packed array of null-terminated strings.
 * @param num_args Number of arguments.
 * @return ExecResult containing the output buffer and execution status.
 * NOTE: The caller must call exec_result_free() on the returned struct.
 */
API ExecResult zmm_sys_exec(char* const* argv, usize num_args);

/**
 * Executes a command, merges output, prints it thread-safely, and frees the
 * output buffer.
 * @return The execution status (error and return code).
 */
API ExecStatus zmm_sys_exec_print(char* const* argv, usize num_args);

/**
 * Executes a command without capturing output (inherits stdio from parent).
 */
API ExecStatus zmm_sys_exec_redirect(char* const* argv, usize num_args);

API ExecResult zmm_sys_exec_flat(const char* arg_buf, usize num_args);

API ExecStatus zmm_sys_exec_print_flat(const char* arg_buf, usize num_args);

API ExecStatus zmm_sys_exec_redirect_flat(const char* arg_buf, usize num_args);
