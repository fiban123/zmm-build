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

#include "args.h"

#include "print.h"
#include "slice.h"

// Initializes the builder using your stack buffers
void zmm_argv_initbuf(ArgvBuilder* cmd, void* flat_buf, usize flat_buf_size,
                      void* argv_buf, usize argv_buf_size) {
    cmd->flat = (char*)flat_buf;
    cmd->flat_cap = flat_buf_size;
    cmd->flat_len = 0;
    cmd->flat_is_heap = false;

    cmd->argv = (char**)argv_buf;
    cmd->argv_cap = argv_buf_size / sizeof(char*);
    cmd->num_args = 0;
    cmd->argv_is_heap = false;
}

// Initializes the builder using your stack buffers
void zmm_argv_initcap(ArgvBuilder* cmd, usize flat_cap, usize argv_cap) {
    cmd->flat = malloc(flat_cap);
    cmd->flat_cap = flat_cap;
    cmd->flat_len = 0;
    cmd->flat_is_heap = true;

    cmd->argv = malloc(argv_cap);
    cmd->argv_cap = argv_cap / sizeof(char*);
    cmd->num_args = 0;
    cmd->argv_is_heap = false;
}

// Internal helper to grow the character buffer safely
static inline void cmd_ensure_flat(ArgvBuilder* cmd, usize needed) {
    if (cmd->flat_len + needed > cmd->flat_cap) {
        usize new_cap = cmd->flat_cap * 2;
        if (new_cap < cmd->flat_len + needed) new_cap = cmd->flat_len + needed;

        char* new_buf = malloc(new_cap);
        memcpy(new_buf, cmd->flat, cmd->flat_len);

        if (cmd->num_args > 0) {
            for (usize i = 0; i < cmd->num_args; i++) {
                size_t offset = cmd->argv[i] - cmd->flat;
                cmd->argv[i] = new_buf + offset;
            }
        }

        if (cmd->flat_is_heap) free(cmd->flat);
        cmd->flat = new_buf;
        cmd->flat_cap = new_cap;
        cmd->flat_is_heap = true;
    }
}

// Internal helper to grow the pointer array safely
static inline void cmd_ensure_argv(ArgvBuilder* cmd, usize needed) {
    if (cmd->num_args + needed > cmd->argv_cap) {
        usize new_cap = cmd->argv_cap * 2;
        if (new_cap < cmd->num_args + needed) new_cap = cmd->num_args + needed;

        char** new_buf = malloc(new_cap * sizeof(char*));
        memcpy(new_buf, cmd->argv, cmd->num_args * sizeof(char*));

        if (cmd->argv_is_heap) free(cmd->argv);
        cmd->argv = new_buf;
        cmd->argv_cap = new_cap;
        cmd->argv_is_heap = true;
    }
}
void zmm_argv_append(ArgvBuilder* cmd, SliceCU8 arg) {
    cmd_ensure_flat(cmd, arg.len + 1);
    cmd_ensure_argv(cmd, 2);

    cmd->argv[cmd->num_args] = cmd->flat + cmd->flat_len;

    memcpy(cmd->flat + cmd->flat_len, arg.ptr, arg.len);
    cmd->flat_len += arg.len;
    cmd->flat[cmd->flat_len++] = '\0';

    cmd->num_args++;
    cmd->argv[cmd->num_args] = NULL;
}

void zmm_argv_append_argv(ArgvBuilder* cmd, const ArgvBuilder* src) {
    if (!src || src->num_args == 0) return;

    // 1. Ensure capacity for the entire payload exactly ONCE
    cmd_ensure_flat(cmd, src->flat_len);
    cmd_ensure_argv(
        cmd, src->num_args + 1);  // +1 to ensure room for the NULL terminator

    // 2. Bulk copy ALL character data in a single memcpy
    memcpy(cmd->flat + cmd->flat_len, src->flat, src->flat_len);

    // 3. Fast-forward the pointers using memory offsets
    for (usize i = 0; i < src->num_args; i++) {
        // Calculate where the string started in the source buffer
        size_t offset = src->argv[i] - src->flat;

        // Map it to the new location in the destination buffer
        cmd->argv[cmd->num_args + i] = cmd->flat + cmd->flat_len + offset;
    }

    // 4. Update the tracker variables
    cmd->flat_len += src->flat_len;
    cmd->num_args += src->num_args;

    // 5. Cap off the pointer array
    cmd->argv[cmd->num_args] = NULL;
}

// Append a null-slice terminated array of slices
void zmm_argv_appendz(ArgvBuilder* cmd, const SliceCU8* args) {
    for (usize i = 0; args[i].ptr != NULL; i++) {
        zmm_argv_append(cmd, args[i]);
    }
}

void zmm_argv_pappend(ArgvBuilder* cmd, SliceCU8 arg) {
    cmd_ensure_flat(cmd, arg.len);
    memcpy(cmd->flat + cmd->flat_len, arg.ptr, arg.len);
    cmd->flat_len += arg.len;
}

void zmm_argv_pfinish(ArgvBuilder* cmd) {
    cmd_ensure_flat(cmd, 1);
    cmd_ensure_argv(cmd, 2);

    // Where did this partial string start?
    // Right after the null terminator of the previous string.
    char* start_ptr = cmd->flat;
    if (cmd->num_args > 0) {
        char* last_arg = cmd->argv[cmd->num_args - 1];
        start_ptr = last_arg + strlen(last_arg) + 1;
    }

    cmd->argv[cmd->num_args] = start_ptr;
    cmd->flat[cmd->flat_len++] = '\0';

    cmd->num_args++;
    cmd->argv[cmd->num_args] = NULL;
}

// Free any heap memory if SBO limits were exceeded
void zmm_argv_free(ArgvBuilder* cmd) {
    if (cmd->flat_is_heap) free(cmd->flat);
    if (cmd->argv_is_heap) free(cmd->argv);

    // Reset to safe zero state
    cmd->flat = NULL;
    cmd->argv = NULL;
    cmd->flat_cap = 0;
    cmd->argv_cap = 0;
    cmd->flat_len = 0;
    cmd->num_args = 0;
}

void zmm_argv_print(char* const* argv, usize num_args) {
    if (num_args == 0) return;

    for (usize i = 0; i < num_args - 1; i++) {
        usize len = strlen(argv[i]);
        zmm_lprintf("%s ", (SliceCU8){.ptr = (const u8*)argv[i], .len = len});
    }
    zmm_lprintf("%sz\n", argv[num_args - 1]);
    zmm_lemit();
}

i32 zmm_args_init(Argv* args, int argc, char** argv) {
    if (argc <= 0 || argv == NULL) {
        return -1;
    }

    args->args = (SliceCU8*)malloc((usize)argc * sizeof(SliceCU8));
    if (!args->args) return -1;

    args->num_args = (usize)argc;
    for (int i = 0; i < argc; i++) {
        args->args[i].ptr = (const u8*)argv[i];
        args->args[i].len = argv[i] ? strlen(argv[i]) : 0;
    }

    return 0;
}

void zmm_args_free(Argv* args) {
    if (args && args->args) {
        free(args->args);
        args->args = NULL;
        args->num_args = 0;
    }
}

bool zmm_args_contains(const Argv* args, SliceCU8 flag) {
    if (!args || !args->args) return false;

    for (usize i = 0; i < args->num_args; i++) {
        if (slice_eq(args->args[i], flag)) {
            return true;
        }
    }

    return false;
}

SliceCU8 zmm_args_get_value(const Argv* args, SliceCU8 key) {
    if (!args || !args->args) return NullSliceCU8;

    for (usize i = 0; i < args->num_args; i++) {
        SliceCU8 arg = args->args[i];

        if (arg.len >= key.len &&
            (key.len == 0 || memcmp(arg.ptr, key.ptr, key.len) == 0)) {
            return (SliceCU8){.ptr = arg.ptr + key.len,
                              .len = arg.len - key.len};
        }
    }

    return NullSliceCU8;
}

SliceCU8 zmm_args_get_subsequent(const Argv* args, SliceCU8 key) {
    if (!args || !args->args) return NullSliceCU8;

    for (usize i = 0; i < args->num_args; i++) {
        if (slice_eq(args->args[i], key)) {
            if (i + 1 < args->num_args) {
                return args->args[i + 1];
            } else {
                return NullSliceCU8;
            }
        }
    }

    return NullSliceCU8;
}
