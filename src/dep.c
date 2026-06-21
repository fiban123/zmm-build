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

#include "dep.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "str.h"
#include "vec.h"

API int zmm_dep_parse(vec(String) * deps, StringView path) {
    if (!deps) return 1;

    char* path_nul = zmm_str_vtoc(path);
    if (!path_nul) return 1;

    FILE* f = fopen(path_nul, "rb");
    free(path_nul);
    if (!f) return 1;

    // 1024 byte fast-path
    char stack_buf[1024];
    char* token_buf = stack_buf;
    usize token_cap = sizeof(stack_buf);
    usize token_len = 0;

    bool seen_colon = false;
    bool escaping = false;
    bool skip_lf = false;

    char chunk[128];
    usize bytes_read;

#define APPEND_CHAR(c)                                   \
    do {                                                 \
        if (token_len >= token_cap) {                    \
            usize new_cap = token_cap * 2;               \
            char* new_buf = malloc(new_cap);             \
            memcpy(new_buf, token_buf, token_len);       \
            if (token_buf != stack_buf) free(token_buf); \
            token_buf = new_buf;                         \
            token_cap = new_cap;                         \
        }                                                \
        token_buf[token_len++] = (c);                    \
    } while (0)

#define COMMIT_TOKEN()                                             \
    do {                                                           \
        if (token_len > 0) {                                       \
            if (seen_colon) {                                      \
                char* new_str = malloc(token_len);                 \
                memcpy(new_str, token_buf, token_len);             \
                String slice = {.ptr = new_str, .len = token_len}; \
                vecpush(*deps, slice);                             \
            }                                                      \
            token_len = 0;                                         \
        }                                                          \
    } while (0)

    while ((bytes_read = fread(chunk, 1, sizeof(chunk), f)) > 0) {
        for (usize i = 0; i < bytes_read; i++) {
            char c = chunk[i];

            if (skip_lf) {
                skip_lf = false;
                if (c == '\n') continue;
            }

            if (escaping) {
                escaping = false;
                if (c == '\n' || c == '\r') {
                    if (c == '\r') skip_lf = true;
                    continue;
                } else {
                    APPEND_CHAR(c);
                    continue;
                }
            }

            if (c == '\\') {
                escaping = true;
            } else if (c == '\n' || c == '\r') {
                COMMIT_TOKEN();
                seen_colon = false;
            } else if (c == ':') {
                bool skip_colon_target = false;

#ifdef _WIN32
                // Only perform Windows drive letter checks (e.g., C:\ or C:/)
                // on Windows
                bool is_drive_letter =
                    (token_len == 1 && isalpha((unsigned char)token_buf[0]));
                if (is_drive_letter) {
                    bool next_is_slash = false;

                    if (i + 1 < bytes_read) {
                        next_is_slash = zmm_p_is_separator((char)chunk[i + 1]);
                    } else {
                        int next_c = fgetc(f);
                        if (next_c != EOF) {
                            next_is_slash = zmm_p_is_separator((char)next_c);
                            ungetc(next_c, f);
                        }
                    }

                    if (next_is_slash) {
                        skip_colon_target = true;
                    }
                }
#endif

                if (!seen_colon && !skip_colon_target) {
                    seen_colon = true;
                    token_len = 0;
                } else {
                    APPEND_CHAR(c);
                }
            } else if (isspace((unsigned char)c)) {
                COMMIT_TOKEN();
            } else {
                APPEND_CHAR(c);
            }
        }
    }

    COMMIT_TOKEN();

    if (token_buf != stack_buf) {
        free(token_buf);
    }

    fclose(f);

    return 0;
}
#undef APPEND_CHAR
#undef COMMIT_TOKEN
