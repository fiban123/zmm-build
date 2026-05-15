#pragma once

#include "export.h"
#include "num.h"
#include "slice.h"

/**
 * A builder for constructing command-line arguments.
 * It is more efficient than fragmented argument dynamic arrays. Additionally,
 * it always keeps track of pointers to the arguments, which means that
 * executing the command requires no extra allocations.
 */
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
 * filled.
 *
 * @param cmd Pointer to the builder to initialize.
 * @param flat_buf Stack buffer for argument strings.
 * @param flat_buf_size Size of flat_buf.
 * @param argv_buf Stack buffer for argument pointers.
 * @param argv_buf_size Size of argv_buf.
 *
 * [FREE] The resulting struct must be freed with zmm_argv_free().
 * [Note] Useful for bypassing heap allocations with stack buffers.
 */
API void zmm_argv_initbuf(ArgvBuilder* cmd, void* flat_buf, usize flat_buf_size,
                          void* argv_buf, usize argv_buf_size);

/**
 * Initializes the ArgvBuilder struct with initial capacities allocated on the
 * heap.
 *
 * @param cmd Pointer to the builder to initialize.
 * @param flat_cap Initial capacity for argument strings.
 * @param argv_cap Initial capacity for argument pointers.
 *
 * [FREE] The resulting struct must be freed with zmm_argv_free().
 */
API void zmm_argv_initcap(ArgvBuilder* cmd, usize flat_cap, usize argv_cap);

/**
 * Appends an argument to the command.
 *
 * @param argv Pointer to the builder.
 * @param arg The argument slice to append.
 */
API void zmm_argv_append(ArgvBuilder* argv, SliceCU8 arg);

/**
 * Appends all arguments from an existing command (ArgvBuilder) to the current
 * command.
 *
 * @param argv Pointer to the destination builder.
 * @param src Pointer to the source builder.
 */
API void zmm_argv_append_argv(ArgvBuilder* argv, const ArgvBuilder* src);

/**
 * Appends a list of arguments to the command.
 *
 * @param argv Pointer to the builder.
 * @param args Nullslice-terminated array of SliceCU8.
 */
API void zmm_argv_appendz(ArgvBuilder* argv, const SliceCU8* args);

/**
 * Appends part of an argument (partial append).
 *
 * @param argv Pointer to the builder.
 * @param arg The argument part to append.
 */
API void zmm_argv_pappend(ArgvBuilder* argv, SliceCU8 arg);

/**
 * Finishes the current partial argument.
 *
 * @param argv Pointer to the builder.
 */
API void zmm_argv_pfinish(ArgvBuilder* argv);

/**
 * Frees the command builder's buffers.
 *
 * @param argv Pointer to the builder to free.
 */
API void zmm_argv_free(ArgvBuilder* argv);

/**
 * Prints the command to stdout thread-safely.
 *
 * @param argv Null-terminated array of argument strings.
 * @param num_args Number of arguments.
 *
 * [Note] Each arg is separated by a space.
 */
void zmm_argv_print(char* const* argv, usize num_args);

/**
 * A structure representing parsed command-line arguments as slices.
 */
typedef struct {
    SliceCU8* args;
    usize num_args;
} Argv;

/**
 * Initializes the Argv struct from standard C argc/argv.
 *
 * @param args Pointer to the Argv struct to initialize.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return 0 on success, -1 on failure.
 *
 * [FREE] The resulting struct must be freed with zmm_args_free().
 */
API i32 zmm_args_init(Argv* args, int argc, char** argv);

/**
 * Frees the Argv struct.
 *
 * @param argv Pointer to the Argv struct to free.
 */
API void zmm_args_free(Argv* argv);

/**
 * Returns whether a flag is present in the arguments.
 *
 * @param argv Pointer to the arguments.
 * @param flag The flag to search for.
 * @return true if the flag is present, false otherwise.
 */
API bool zmm_args_contains(const Argv* argv, SliceCU8 flag);

/**
 * Returns the value of a conjoined argument like `--depth=100`.
 *
 * @param argv Pointer to the arguments.
 * @param key The key to search for (including the equal sign, e.g., "--port=").
 * @return Slice containing the value, or a nullslice if not found.
 */
API SliceCU8 zmm_args_get_value(const Argv* argv, SliceCU8 key);

/**
 * Returns the value of a separated argument like `--port 8080`.
 *
 * @param argv Pointer to the arguments.
 * @param key The key to search for.
 * @return Slice containing the value, or a nullslice if not found.
 */
API SliceCU8 zmm_args_get_subsequent(const Argv* argv, SliceCU8 key);
