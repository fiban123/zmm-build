/*
 * zmm-build: A compiled C build system
 * Copyright 2026 fiban123
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "print.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "str.h"

// -----------------------------------------------------------------------------
// Writer Definition
// -----------------------------------------------------------------------------
typedef struct {
    char* buf;
    usize size;
    usize written;
    FILE* stream;

    char* flush_buf;
    usize flush_cap;
    usize flush_len;

    bool is_tl;
} Writer;

// -----------------------------------------------------------------------------
// Thread-Local Storage (Flattened for Zero-Branch Access)
// -----------------------------------------------------------------------------
#define TL_INITIAL_CAP 4096

static _Thread_local char tl_static_buf[TL_INITIAL_CAP];
static _Thread_local char* tl_active_buf = NULL;
static _Thread_local usize tl_cap = TL_INITIAL_CAP;
static _Thread_local usize tl_len = 0;

static pthread_mutex_t lemit_mutex = PTHREAD_MUTEX_INITIALIZER;

// -----------------------------------------------------------------------------
// Core Block Writer
// -----------------------------------------------------------------------------
static inline void zmm_write_block(Writer* w, const char* data, usize len) {
    if (len == 0) return;

    if (w->stream) {
        if (w->flush_len + len >= w->flush_cap) {
            if (w->flush_len > 0) {
                fwrite(w->flush_buf, 1, w->flush_len, w->stream);
                w->flush_len = 0;
            }
            if (len >= w->flush_cap) {
                fwrite(data, 1, len, w->stream);
            } else {
                memcpy(w->flush_buf, data, len);
                w->flush_len = len;
            }
        } else {
            memcpy(w->flush_buf + w->flush_len, data, len);
            w->flush_len += len;
        }
    } else if (w->is_tl) {
        if (!tl_active_buf) tl_active_buf = tl_static_buf;

        usize needed = tl_len + len;
        if (needed > tl_cap) {
            usize new_cap = tl_cap * 2;
            while (new_cap < needed) new_cap *= 2;

            char* new_buf = malloc(new_cap);
            if (!new_buf) return;  // OOM failsafe

            memcpy(new_buf, tl_active_buf, tl_len);

            // Only free if we were already using the heap
            if (tl_active_buf != tl_static_buf) free(tl_active_buf);

            tl_active_buf = new_buf;
            tl_cap = new_cap;
        }

        // Zero-branching write
        memcpy(tl_active_buf + tl_len, data, len);
        tl_len += len;
    } else if (w->buf) {
        usize space_left = 0;
        // Removed the `- 1` since we are no longer reserving space for \0
        if (w->written < w->size) {
            space_left = w->size - w->written;
        }
        usize copy_len = len < space_left ? len : space_left;
        if (copy_len > 0) {
            memcpy(w->buf + w->written, data, copy_len);
        }
    }

    w->written += len;
}

// -----------------------------------------------------------------------------
// Formatters
// -----------------------------------------------------------------------------

// Base-100 Lookup Table: Halves the number of expensive modulo/division
// operations
static const char DIGIT_PAIRS[201] =
    "0001020304050607080910111213141516171819"
    "2021222324252627282930313233343536373839"
    "4041424344454647484950515253545556575859"
    "6061626364656667686970717273747576777879"
    "8081828384858687888990919293949596979899";

static inline void format_u64(Writer* w, u64 val) {
    char buf[24];
    int i = sizeof(buf);

    if (val == 0) {
        buf[--i] = '0';
    } else {
        while (val >= 100) {
            unsigned const int idx = (val % 100) * 2;
            val /= 100;
            buf[--i] = DIGIT_PAIRS[idx + 1];
            buf[--i] = DIGIT_PAIRS[idx];
        }
        if (val < 10) {
            buf[--i] = '0' + (char)val;
        } else {
            unsigned const int idx = val * 2;
            buf[--i] = DIGIT_PAIRS[idx + 1];
            buf[--i] = DIGIT_PAIRS[idx];
        }
    }
    zmm_write_block(w, &buf[i], sizeof(buf) - i);
}

static inline void format_i64(Writer* w, i64 val) {
    if (val < 0) {
        zmm_write_block(w, "-", 1);
        val = -val;
    }
    format_u64(w, (u64)val);
}

static inline void format_hex(Writer* w, u64 val) {
    char buf[24];
    int i = sizeof(buf);
    const char* hex_chars = "0123456789abcdef";

    if (val == 0) {
        buf[--i] = '0';
    } else {
        while (val > 0) {
            buf[--i] = hex_chars[val & 0xF];
            val >>= 4;  // Bitwise shift is much faster than division
        }
    }
    zmm_write_block(w, &buf[i], sizeof(buf) - i);
}

static inline void format_f64(Writer* w, double val, int precision) {
    if (val < 0) {
        zmm_write_block(w, "-", 1);
        val = -val;
    }

    u64 int_part = (u64)val;
    format_u64(w, int_part);

    if (precision > 0) {
        zmm_write_block(w, ".", 1);
        double frac = val - (double)int_part;

        // Calculate multiplier for precision (e.g., 2 -> 100)
        u64 mult = 1;
        for (int i = 0; i < precision; i++) mult *= 10;

        // Multiply and add 0.5 for standard rounding
        u64 frac_part = (u64)(frac * mult + 0.5);

        // Output fractional part, manually padding leading zeros
        char buf[16];  // Max precision supported here is 16
        for (int i = precision - 1; i >= 0; i--) {
            buf[i] = '0' + (frac_part % 10);
            frac_part /= 10;
        }
        zmm_write_block(w, buf, precision);
    }
}

// -----------------------------------------------------------------------------
// Core Parser (Vectorized Literal Scanning + O(1) Switch)
// -----------------------------------------------------------------------------
static usize zmm_vformat(Writer* w, const char* format, va_list args) {
    const char* f = format;

    while (*f != '\0') {
        const char* next = strchr(f, '%');
        if (!next) {
            zmm_write_block(w, f, strlen(f));
            break;
        }

        if (next > f) {
            zmm_write_block(w, f, (usize)(next - f));
        }

        f = next + 1;  // Move past the '%'

        if (*f == '%') {
            zmm_write_block(w, "%", 1);
            f++;
            continue;
        }

        int precision = 6;  // default precision
        if (*f == '.') {
            f++;
            precision = 0;
            while (*f >= '0' && *f <= '9') {
                precision = precision * 10 + (*f - '0');
                f++;
            }
        }

        switch (*f) {
            case 'i':
                if (f[1] == '3' && f[2] == '2') {
                    format_i64(w, va_arg(args, i32));
                    f += 3;
                } else if (f[1] == '6' && f[2] == '4') {
                    format_i64(w, va_arg(args, i64));
                    f += 3;
                } else if (f[1] == '1' && f[2] == '6') {
                    format_i64(w, (i16)va_arg(args, int));
                    f += 3;
                } else if (f[1] == '8') {
                    format_i64(w, (i8)va_arg(args, int));
                    f += 2;
                } else {
                    zmm_write_block(w, "%i", 2);
                    f++;
                }
                break;
            case 'u':
                if (f[1] == 's' && f[2] == 'z') {
                    format_u64(w, va_arg(args, usize));
                    f += 3;
                } else if (f[1] == '3' && f[2] == '2') {
                    format_u64(w, va_arg(args, u32));
                    f += 3;
                } else if (f[1] == '6' && f[2] == '4') {
                    format_u64(w, va_arg(args, u64));
                    f += 3;
                } else if (f[1] == '1' && f[2] == '6') {
                    format_u64(w, (u16)va_arg(args, unsigned int));
                    f += 3;
                } else if (f[1] == '8') {
                    format_u64(w, (u8)va_arg(args, unsigned int));
                    f += 2;
                } else {
                    zmm_write_block(w, "%u", 2);
                    f++;
                }
                break;
            case 's':
                if (f[1] == 'z') {
                    const char* p = va_arg(args, const char*);
                    if (!p)
                        zmm_write_block(w, "(null)", 6);
                    else
                        zmm_write_block(w, p, strlen(p));
                    f += 2;
                } else if (f[1] == 's' && f[2] == 'z') {
                    format_i64(w, va_arg(args, ssize));
                    f += 3;
                } else {
                    // Replaced SliceCU8 with StringView
                    StringView sv = va_arg(args, StringView);
                    if (!sv.ptr)
                        zmm_write_block(w, "(null)", 6);
                    else
                        zmm_write_block(w, sv.ptr, sv.len);
                    f += 1;
                }
                break;
            case 'c': {
                char c = (char)va_arg(args, int);
                zmm_write_block(w, &c, 1);
                f++;
                break;
            }
            case 'd':
                format_i64(w, va_arg(args, int));
                f++;
                break;
            case 'x':
                format_hex(w, va_arg(args, unsigned int));
                f++;
                break;
            case 'p': {
                void* ptr = va_arg(args, void*);
                zmm_write_block(w, "0x", 2);
                format_hex(w, (u64)(uintptr_t)ptr);
                f++;
                break;
            }
            case 'b': {
                bool b = (bool)va_arg(args, int);
                if (b)
                    zmm_write_block(w, "true", 4);
                else
                    zmm_write_block(w, "false", 5);
                f++;
                break;
            }
            case 'f':
                format_f64(w, va_arg(args, double), precision);
                f++;
                break;
            default:
                zmm_write_block(w, "%", 1);
                zmm_write_block(w, f, 1);
                f++;
                break;
        }
    }

    if (w->stream && w->flush_len > 0) {
        fwrite(w->flush_buf, 1, w->flush_len, w->stream);
        w->flush_len = 0;
    }

    // Null termination block cleanly removed

    return w->written;
}

API StringView zmm_snprintf(String str, const char* format, ...) {
    Writer w = {.buf = str.ptr, .size = str.len, .written = 0};
    va_list args;
    va_start(args, format);
    usize ret = zmm_vformat(&w, format, args);
    va_end(args);

    // Clamp the returned view length so it reflects the actual written bytes
    usize actual_len = ret < str.len ? ret : str.len;
    return (StringView){.ptr = str.ptr, .len = actual_len};
}

API StringView zmm_sprintf(String str, const char* format, ...) {
    Writer w = {.buf = str.ptr, .size = str.len, .written = 0};
    va_list args;
    va_start(args, format);
    usize ret = zmm_vformat(&w, format, args);
    va_end(args);

    usize actual_len = ret < str.len ? ret : str.len;
    return (StringView){.ptr = str.ptr, .len = actual_len};
}

API usize zmm_printf(const char* format, ...) {
    char stack_buf[1024];
    Writer w = {.stream = stdout,
                .flush_buf = stack_buf,
                .flush_cap = sizeof(stack_buf),
                .flush_len = 0};
    va_list args;
    va_start(args, format);
    usize ret = zmm_vformat(&w, format, args);
    va_end(args);
    return ret;
}

API usize zmm_lprintf(const char* format, ...) {
    Writer w = {.is_tl = true};
    va_list args;
    va_start(args, format);
    usize ret = zmm_vformat(&w, format, args);
    va_end(args);
    return ret;
}

API void zmm_lemit(void) {
    if (tl_len == 0) return;

    pthread_mutex_lock(&lemit_mutex);
    fwrite(tl_active_buf, 1, tl_len, stdout);
    fflush(stdout);
    pthread_mutex_unlock(&lemit_mutex);

    tl_len = 0;
}
