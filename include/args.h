#include "cc.h"
#include "export.h"
#include "num.h"
#include "slice.h"

typedef struct {
    char* args;
    usize args_cap;
    usize args_len;
    bool args_is_heap;

    char** argptr_buf;
    usize argptr_cap;
    usize num_args;
    bool argptrs_is_heap;
} ArgvBuilder;

/**
 * Initializes the ArgvBuilder struct with provided buffers.
 * The builder uses these buffers first and falls back to heap if they are
 * filled. Useful for bypassing heap allocations with stack buffers. NOTE: The
 * resulting struct must be freed with zmm_cmd_free().
 */
API void zmm_cmd_initbuf(ArgvBuilder* cmd, void* argbuf, usize argbuf_size,
                         void* argptr_buf, usize argptr_buf_size);

/**
 * Appends an argument to the command.
 */
API void zmm_cmd_append(ArgvBuilder* cmd, SliceCU8 arg);

/**
 * Appends a list of arguments to the command.
 * @param args Null-terminated array of SliceCU8.
 */
API void zmm_cmd_appendz(ArgvBuilder* cmd, const SliceCU8* args);

/**
 * Appends part of an argument (partial append).
 */
API void zmm_cmd_pappend(ArgvBuilder* cmd, SliceCU8 arg);

/**
 * Finishes the current partial argument.
 */
API void zmm_cmd_pfinish(ArgvBuilder* cmd);

/**
 * Frees the command builder's buffers.
 */
API void zmm_cmd_free(ArgvBuilder* cmd);

typedef struct {
    SliceCU8* ptr;
    usize len;
} Argv;

/**
 * Initializes the Argv struct from standard C argc/argv.
 * @return 0 on success, -1 on failure.
 * NOTE: The resulting struct must be freed with zmm_arg_free().
 */
API i32 zmm_arg_init(Argv* self, int argc, char** argv);

/**
 * Frees the Argv struct.
 */
API void zmm_arg_free(Argv* self);

/**
 * Returns whether a flag is present in the arguments.
 */
API bool zmm_arg_contains(const Argv* self, SliceCU8 flag);

/**
 * Returns the value of a conjoined argument like `--port=8080`.
 * Pass the key with the equal sign (e.g., "--port=").
 */
API SliceCU8 zmm_arg_get_value(const Argv* self, SliceCU8 key);

/**
 * Returns the value of a separated argument like `--port 8080`.
 */
API SliceCU8 zmm_arg_get_subsequent(const Argv* self, SliceCU8 key);
