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
#include "str.h"

// Initializes the builder using your stack buffers
API int zmm_argv_initcap(ArgvBuilder* cmd, usize buf_cap, usize argv_cap) {
    cmd->buf = malloc(buf_cap);
    cmd->buf_len = 0;
    cmd->buf_cap = buf_cap;

    cmd->argv = malloc(argv_cap * sizeof(char*));
    cmd->argv_len = 0;
    cmd->argv_cap = argv_cap;

    return (!cmd->buf) || (!cmd->argv);
}

static inline int ensure_buf(ArgvBuilder* argv, usize needed) {
    if (argv->buf_len + needed > argv->buf_cap) {
        argv->buf_cap = 1;
        while (argv->buf_len + needed > argv->buf_cap) {
            argv->buf_cap *= 2;
        }

        argv->buf = realloc(argv->buf, argv->buf_cap);
    }

    return !argv->buf;
}

static inline int ensure_argv(ArgvBuilder* argv, usize needed) {
    if (argv->argv_len + needed > argv->argv_cap) {
        argv->argv_cap = 1;
        while (argv->argv_len + needed > argv->argv_cap) {
            argv->argv_cap *= 2;
        }

        argv->argv = realloc(argv->argv, argv->argv_cap);
    }

    return !argv->argv;
}

API int zmm_argv_append(ArgvBuilder* argv, StringView arg) {
    if (ensure_buf(argv, arg.len + 1)) return 1;
    if (ensure_argv(argv, 1)) return 1;

    memcpy(argv->buf + argv->buf_len, arg.ptr, arg.len);
    argv->argv[argv->argv_len++] = argv->buf + argv->buf_len;
    argv->buf_len += arg.len;
    argv->buf[argv->buf_len++] = '\n';

    return 0;
}

API int zmm_argv_append_argv(ArgvBuilder* argv, const ArgvBuilder* src) {
    if (ensure_buf(argv, src->buf_len)) return 1;
    if (ensure_argv(argv, src->argv_len)) return 1;

    for (usize i = 0; i < src->argv_len; i++) {
        zmm_argv_append(argv, (StringView){src->argv[i], strlen(src->argv[i])});
    }

    return 0;
}

API int zmm_argv_pstart(ArgvBuilder* argv) {
    if (ensure_argv(argv, 1)) return 1;

    argv->argv[argv->argv_len++] = argv->buf + argv->buf_len;

    return 0;
}

API int zmm_argv_pappend(ArgvBuilder* argv, StringView arg) {
    if (ensure_buf(argv, arg.len)) return 1;

    memcpy(argv->buf + argv->buf_len, arg.ptr, arg.len);
    argv->buf_len += arg.len;

    return 0;
}

API int zmm_argv_pfinish(ArgvBuilder* argv) {
    if (ensure_buf(argv, 1)) return 1;

    argv->buf[argv->buf_len++] = '\n';

    return 0;
}

void zmm_argv_free(ArgvBuilder* argv) {
    free(argv->argv);
    free(argv->buf);
}

void zmm_argv_print(ArgvBuilder* argv) {
    if (argv->argv_len == 0) return;

    for (usize i = 0; i < argv->argv_len - 1; i++) {
        usize len = strlen(argv->argv[i]);
        zmm_lprintf("%s ", (StringView){argv->argv[i], len});
    }
    zmm_lprintf("%sz\n", argv->argv[argv->argv_len - 1]);
    zmm_lemit();
}
