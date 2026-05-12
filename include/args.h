#pragma once

#include "export.h"
#include "num.h"
#include "slice.h"

typedef struct {
    char* flat;
    usize flat_cap;
    usize flat_len;
    bool flat_is_heap;

    char** argv;
    usize argv_cap;
    usize num_args;
    bool argv_is_heap;
} ArgvBuilder;

/**
 * Initializes the ArgvBuilder struct with provided buffers.
 * The builder uses these buffers first and falls back to heap if they are
 * filled. Useful for bypassing heap allocations with stack buffers. NOTE: The
 * resulting struct must be freed with zmm_cmd_free().
 */
API void zmm_argv_initbuf(ArgvBuilder* cmd, void* flat_buf, usize flat_buf_size,
                          void* argv_buf, usize argv_buf_size);

API void zmm_argv_initcap(ArgvBuilder* cmd, usize flat_cap, usize argv_cap);

/**
 * Appends an argument to the command.
 */
API void zmm_argv_append(ArgvBuilder* argv, SliceCU8 arg);

/**
 * Appends all arguments from an existing command (ArgvBuilder) to the current
 * command.
 */
API void zmm_argv_append_argv(ArgvBuilder* argv, const ArgvBuilder* src);

/**
 * Appends a list of arguments to the command.
 * @param args Null-terminated array of SliceCU8.
 */
API void zmm_argv_appendz(ArgvBuilder* argv, const SliceCU8* args);

/**
 * Appends part of an argument (partial append).
 */
API void zmm_argv_pappend(ArgvBuilder* argv, SliceCU8 arg);

/**
 * Finishes the current partial argument.
 */
API void zmm_argv_pfinish(ArgvBuilder* argv);

/**
 * Frees the command builder's buffers.
 */
API void zmm_argv_free(ArgvBuilder* argv);

/**
 * Prints the command to stdout thread-safely.
 * each arg is separated by a space
 */
void zmm_argv_print(char* const* argv, usize num_args);

typedef struct {
    SliceCU8* ptr;
    usize len;
} Argv;

/**
 * Initializes the Argv struct from standard C argc/argv.
 * @return 0 on success, -1 on failure.
 * NOTE: The resulting struct must be freed with zmm_arg_free().
 */
API i32 zmm_args_init(Argv* args, int argc, char** argv);

/**
 * Frees the Argv struct.
 */
API void zmm_args_free(Argv* argv);

/**
 * Returns whether a flag is present in the arguments.
 */
API bool zmm_args_contains(const Argv* argv, SliceCU8 flag);

/**
 * Returns the value of a conjoined argument like `--port=8080`.
 * Pass the key with the equal sign (e.g., "--port=").
 */
API SliceCU8 zmm_args_get_value(const Argv* argv, SliceCU8 key);

/**
 * Returns the value of a separated argument like `--port 8080`.
 */
API SliceCU8 zmm_args_get_subsequent(const Argv* argv, SliceCU8 key);
