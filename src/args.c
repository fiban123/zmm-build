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
API int zmm_argvb_initcap(ArgvBuilder* cmd, usize buf_cap, usize argv_cap) {
    cmd->buf = malloc(buf_cap);
    cmd->buf_len = 0;
    cmd->buf_cap = buf_cap;

    cmd->argv = malloc(argv_cap * sizeof(char*));
    cmd->argv_len = 0;
    cmd->argv_cap = argv_cap;

    return (!cmd->buf) || (!cmd->argv);
}

static inline int ensure_buf(ArgvBuilder* argvb, usize needed) {
    if (argvb->buf_len + needed > argvb->buf_cap) {
        argvb->buf_cap = 1;
        while (argvb->buf_len + needed > argvb->buf_cap) {
            argvb->buf_cap *= 2;
        }

        argvb->buf = realloc(argvb->buf, argvb->buf_cap);
    }

    return !argvb->buf;
}

static inline int ensure_argv(ArgvBuilder* argvb, usize needed) {
    if (argvb->argv_len + needed > argvb->argv_cap) {
        argvb->argv_cap = 1;
        while (argvb->argv_len + needed > argvb->argv_cap) {
            argvb->argv_cap *= 2;
        }

        argvb->argv = realloc(argvb->argv, argvb->argv_cap);
    }

    return !argvb->argv;
}

API int zmm_argvb_append(ArgvBuilder* argvb, StringView arg) {
    if (ensure_buf(argvb, arg.len + 1)) return 1;
    if (ensure_argv(argvb, 1)) return 1;

    memcpy(argvb->buf + argvb->buf_len, arg.ptr, arg.len);
    argvb->argv[argvb->argv_len++] = argvb->buf + argvb->buf_len;
    argvb->buf_len += arg.len;
    argvb->buf[argvb->buf_len++] = '\0';

    return 0;
}

API int zmm_argvb_appendc(ArgvBuilder* argvb, const char* arg) {
    return zmm_argvb_append(argvb, zmm_str_ctov(arg));
}

API int zmm_argvb_append_n(ArgvBuilder* argvb, const StringView* args,
                           usize n) {
    for (usize i = 0; i < n; i++) {
        if (zmm_argvb_append(argvb, args[i])) return 1;
    }

    return 0;
}

API int zmm_argvb_appendc_n(ArgvBuilder* argvb, const char* const* args,
                            usize n) {
    for (usize i = 0; i < n; i++) {
        if (zmm_argvb_appendc(argvb, args[i])) return 1;
    }

    return 0;
}

API int zmm_argvb_append_argv(ArgvBuilder* argvb, const char* const* argv,
                              usize argv_len) {
    for (usize i = 0; i < argv_len; i++) {
        zmm_argvb_append(argvb, (StringView){argv[i], strlen(argv[i])});
    }

    return 0;
}

API int zmm_argvb_pstart(ArgvBuilder* argvb) {
    if (ensure_argv(argvb, 1)) return 1;

    argvb->argv[argvb->argv_len++] = argvb->buf + argvb->buf_len;

    return 0;
}

API int zmm_argvb_pappend(ArgvBuilder* argvb, StringView arg) {
    if (ensure_buf(argvb, arg.len)) return 1;

    memcpy(argvb->buf + argvb->buf_len, arg.ptr, arg.len);
    argvb->buf_len += arg.len;

    return 0;
}

API int zmm_argvb_pfinish(ArgvBuilder* argvb) {
    if (ensure_buf(argvb, 1)) return 1;

    argvb->buf[argvb->buf_len++] = '\0';

    return 0;
}

API void zmm_argvb_free(ArgvBuilder* argvb) {
    free(argvb->argv);
    free(argvb->buf);
}

API void zmm_argv_print(char* const* argv, usize argv_len) {
    if (argv_len == 0) return;

    for (usize i = 0; i < argv_len - 1; i++) {
        zmm_lprintf("%sz ", argv[i]);
    }
    zmm_lprintf("%sz\n", argv[argv_len - 1]);
    zmm_lemit();
}
