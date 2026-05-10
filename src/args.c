#include "args.h"

#include "print.h"
#include "slice.h"

// Initializes the builder using your stack buffers
void zmm_cmd_initbuf(ArgvBuilder* cmd, void* cbuf, usize cbuf_size, void* abuf,
                     usize abuf_size) {
    cmd->args = (char*)cbuf;
    cmd->args_cap = cbuf_size;
    cmd->args_len = 0;
    cmd->args_is_heap = false;

    cmd->argptr_buf = (char**)abuf;
    cmd->argptr_cap = abuf_size / sizeof(char*);
    cmd->num_args = 0;
    cmd->argptrs_is_heap = false;
}

// Internal helper to grow the character buffer safely
static void cmd_ensure_char(ArgvBuilder* cmd, usize needed) {
    if (cmd->args_len + needed > cmd->args_cap) {
        usize new_cap = cmd->args_cap * 2;
        if (new_cap < cmd->args_len + needed) new_cap = cmd->args_len + needed;

        char* new_buf = malloc(new_cap);
        memcpy(new_buf, cmd->args, cmd->args_len);

        if (cmd->num_args > 0) {
            for (usize i = 0; i < cmd->num_args; i++) {
                size_t offset = cmd->argptr_buf[i] - cmd->args;
                cmd->argptr_buf[i] = new_buf + offset;
            }
        }

        if (cmd->args_is_heap) free(cmd->args);
        cmd->args = new_buf;
        cmd->args_cap = new_cap;
        cmd->args_is_heap = true;
    }
}

// Internal helper to grow the pointer array safely
static void cmd_ensure_arg(ArgvBuilder* cmd, usize needed) {
    if (cmd->num_args + needed > cmd->argptr_cap) {
        usize new_cap = cmd->argptr_cap * 2;
        if (new_cap < cmd->num_args + needed) new_cap = cmd->num_args + needed;

        char** new_buf = malloc(new_cap * sizeof(char*));
        memcpy(new_buf, cmd->argptr_buf, cmd->num_args * sizeof(char*));

        if (cmd->argptrs_is_heap) free(cmd->argptr_buf);
        cmd->argptr_buf = new_buf;
        cmd->argptr_cap = new_cap;
        cmd->argptrs_is_heap = true;
    }
}
void zmm_cmd_append(ArgvBuilder* cmd, SliceCU8 arg) {
    cmd_ensure_char(cmd, arg.len + 1);
    cmd_ensure_arg(cmd, 2);

    cmd->argptr_buf[cmd->num_args] = cmd->args + cmd->args_len;

    memcpy(cmd->args + cmd->args_len, arg.ptr, arg.len);
    cmd->args_len += arg.len;
    cmd->args[cmd->args_len++] = '\0';

    cmd->num_args++;
    cmd->argptr_buf[cmd->num_args] = NULL;
}

// Append a null-slice terminated array of slices
void zmm_cmd_appendz(ArgvBuilder* cmd, const SliceCU8* args) {
    for (usize i = 0; args[i].ptr != NULL; i++) {
        zmm_cmd_append(cmd, args[i]);
    }
}

void zmm_cmd_pappend(ArgvBuilder* cmd, SliceCU8 arg) {
    cmd_ensure_char(cmd, arg.len);
    memcpy(cmd->args + cmd->args_len, arg.ptr, arg.len);
    cmd->args_len += arg.len;
}

void zmm_cmd_pfinish(ArgvBuilder* cmd) {
    cmd_ensure_char(cmd, 1);
    cmd_ensure_arg(cmd, 2);

    // Where did this partial string start?
    // Right after the null terminator of the previous string.
    char* start_ptr = cmd->args;
    if (cmd->num_args > 0) {
        char* last_arg = cmd->argptr_buf[cmd->num_args - 1];
        start_ptr = last_arg + strlen(last_arg) + 1;
    }

    cmd->argptr_buf[cmd->num_args] = start_ptr;
    cmd->args[cmd->args_len++] = '\0';

    cmd->num_args++;
    cmd->argptr_buf[cmd->num_args] = NULL;
}

// Free any heap memory if SBO limits were exceeded
void zmm_cmd_free(ArgvBuilder* cmd) {
    if (cmd->args_is_heap) free(cmd->args);
    if (cmd->argptrs_is_heap) free(cmd->argptr_buf);

    // Reset to safe zero state
    cmd->args = NULL;
    cmd->argptr_buf = NULL;
    cmd->args_cap = 0;
    cmd->argptr_cap = 0;
    cmd->args_len = 0;
    cmd->num_args = 0;
}

void zmm_cmd_print(const char* args, usize num_args) {
    const char* p = args;
    for (usize i = 0; i < num_args - 1; i++) {
        usize len = strlen(p);
        zmm_lprintf("%s ", (SliceCU8){.ptr = (const u8*)p, .len = len});
        p += len + 1;
    }
    zmm_lprintf("%sz\n", p);
    zmm_lemit();
}

i32 zmm_arg_init(Argv* self, int argc, char** argv) {
    if (argc <= 0 || argv == NULL) {
        return -1;
    }

    self->ptr = (SliceCU8*)malloc((usize)argc * sizeof(SliceCU8));
    if (!self->ptr) return -1;

    self->len = (usize)argc;
    for (int i = 0; i < argc; i++) {
        self->ptr[i].ptr = (const u8*)argv[i];
        self->ptr[i].len = argv[i] ? strlen(argv[i]) : 0;
    }

    return 0;
}

void zmm_arg_free(Argv* self) {
    if (self && self->ptr) {
        free(self->ptr);
        self->ptr = NULL;
        self->len = 0;
    }
}

bool zmm_arg_contains(const Argv* self, SliceCU8 flag) {
    if (!self || !self->ptr) return false;

    for (usize i = 0; i < self->len; i++) {
        if (slice_eq(self->ptr[i], flag)) {
            return true;
        }
    }

    return false;
}

SliceCU8 zmm_arg_get_value(const Argv* self, SliceCU8 key) {
    if (!self || !self->ptr) return NullSliceCU8;

    for (usize i = 0; i < self->len; i++) {
        SliceCU8 arg = self->ptr[i];

        // Equivalent to std.mem.startsWith
        if (arg.len >= key.len &&
            (key.len == 0 || memcmp(arg.ptr, key.ptr, key.len) == 0)) {
            return (SliceCU8){.ptr = arg.ptr + key.len,
                              .len = arg.len - key.len};
        }
    }

    return NullSliceCU8;  // error.KeyNotFound
}

SliceCU8 zmm_arg_get_subsequent(const Argv* self, SliceCU8 key) {
    if (!self || !self->ptr) return NullSliceCU8;

    for (usize i = 0; i < self->len; i++) {
        if (slice_eq(self->ptr[i], key)) {
            // Check if a subsequent argument actually exists
            if (i + 1 < self->len) {
                return self->ptr[i + 1];
            } else {
                return NullSliceCU8;  // error.MissingValue
            }
        }
    }

    return NullSliceCU8;  // error.KeyNotFound
}
