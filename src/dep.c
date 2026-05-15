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

#include "dep.h"

#include <ctype.h>
#include <stb/stb_ds.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "path.h"
#include "slice.h"

i32 zmm_dep_parse(arr(SliceU8) * deps, SliceCU8 path) {
    if (!deps) return -1;

    char* path_nul = slice_to_cstr(path);
    if (!path_nul) return -1;

    FILE* f = fopen(path_nul, "rb");
    free(path_nul);
    if (!f) return -1;

    // 1024 byte fast-path
    u8 stack_buf[1024];
    u8* token_buf = stack_buf;
    usize token_cap = sizeof(stack_buf);
    usize token_len = 0;

    bool seen_colon = false;
    bool escaping = false;
    bool skip_lf = false;

    u8 chunk[128];
    usize bytes_read;

#define APPEND_CHAR(c)                                   \
    do {                                                 \
        if (token_len >= token_cap) {                    \
            usize new_cap = token_cap * 2;               \
            u8* new_buf = (u8*)malloc(new_cap);          \
            memcpy(new_buf, token_buf, token_len);       \
            if (token_buf != stack_buf) free(token_buf); \
            token_buf = new_buf;                         \
            token_cap = new_cap;                         \
        }                                                \
        token_buf[token_len++] = (c);                    \
    } while (0)

#define COMMIT_TOKEN()                                              \
    do {                                                            \
        if (token_len > 0) {                                        \
            if (seen_colon) {                                       \
                u8* new_str = (u8*)malloc(token_len);               \
                memcpy(new_str, token_buf, token_len);              \
                SliceU8 slice = {.ptr = new_str, .len = token_len}; \
                arrput(*deps, slice);                               \
            }                                                       \
            token_len = 0;                                          \
        }                                                           \
    } while (0)

    while ((bytes_read = fread(chunk, 1, sizeof(chunk), f)) > 0) {
        for (usize i = 0; i < bytes_read; i++) {
            u8 c = chunk[i];

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
