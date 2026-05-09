#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

#include "print.h"  // Assuming your types are here

#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "slice.h"

// -----------------------------------------------------------------------------
// Writer Definition
// -----------------------------------------------------------------------------
typedef struct {
    char* buf;
    usize size;
    usize written;
    FILE* stream;

    // Stack-buffering for I/O (to speed up stdout)
    char* flush_buf;
    usize flush_cap;
    usize flush_len;

    // Flag for thread-local accumulator
    bool is_tl;
} ZmmWriter;

// -----------------------------------------------------------------------------
// Thread-Local Storage & Mutex
// -----------------------------------------------------------------------------
#define TL_INITIAL_CAP 4096

// Thread-local variables (each thread gets its own instance of these)
static _Thread_local char tl_static_buf[TL_INITIAL_CAP];
static _Thread_local char* tl_dyn_buf =
    NULL;  // Points to heap if we exceed 4096
static _Thread_local usize tl_cap = TL_INITIAL_CAP;
static _Thread_local usize tl_len = 0;

static pthread_mutex_t lemit_mutex = PTHREAD_MUTEX_INITIALIZER;

// -----------------------------------------------------------------------------
// Core Block Writer (The Engine)
// -----------------------------------------------------------------------------
static inline void zmm_write_block(ZmmWriter* w, const char* data, usize len) {
    if (len == 0) return;

    if (w->stream) {
        // Fast-path: Write to stack buffer first to minimize I/O system calls
        if (w->flush_len + len >= w->flush_cap) {
            // Flush existing buffer
            if (w->flush_len > 0) {
                fwrite(w->flush_buf, 1, w->flush_len, w->stream);
                w->flush_len = 0;
            }
            // If the incoming chunk is huge, bypass the buffer entirely
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
        // Thread-local dynamic sizing logic
        usize needed = tl_len + len;
        if (needed > tl_cap) {
            usize new_cap = tl_cap * 2;
            while (new_cap < needed) new_cap *= 2;

            char* new_buf;
            if (tl_dyn_buf) {
                new_buf = realloc(tl_dyn_buf, new_cap);
            } else {
                new_buf = malloc(new_cap);
                if (new_buf) memcpy(new_buf, tl_static_buf, tl_len);
            }

            if (new_buf) {
                tl_dyn_buf = new_buf;
                tl_cap = new_cap;
            } else {
                return;  // OOM failsafe, silently drop
            }
        }

        char* dest = tl_dyn_buf ? tl_dyn_buf : tl_static_buf;
        memcpy(dest + tl_len, data, len);
        tl_len += len;
    } else if (w->buf) {
        // Standard memory buffer logic (snprintf)
        usize space_left = 0;
        if (w->written < w->size - 1) {
            space_left = w->size - 1 - w->written;
        }

        usize copy_len = len < space_left ? len : space_left;
        if (copy_len > 0) {
            memcpy(w->buf + w->written, data, copy_len);
        }
    }

    w->written += len;
}

// -----------------------------------------------------------------------------
// Formatters (Now using stack buffers)
// -----------------------------------------------------------------------------

static inline void format_u64(ZmmWriter* w, u64 val) {
    char buf[24];
    int i = sizeof(buf);

    if (val == 0) {
        buf[--i] = '0';
    } else {
        while (val > 0) {
            buf[--i] = '0' + (val % 10);
            val /= 10;
        }
    }

    zmm_write_block(w, &buf[i], sizeof(buf) - i);
}

static inline void format_i64(ZmmWriter* w, i64 val) {
    if (val < 0) {
        zmm_write_block(w, "-", 1);
        u64 uval = (u64)(-(val + 1)) + 1;
        format_u64(w, uval);
    } else {
        format_u64(w, (u64)val);
    }
}

static inline void format_hex(ZmmWriter* w, u64 val) {
    char buf[24];
    int i = sizeof(buf);
    const char* hex_chars = "0123456789abcdef";

    if (val == 0) {
        buf[--i] = '0';
    } else {
        while (val > 0) {
            buf[--i] = hex_chars[val & 0xF];
            val >>= 4;
        }
    }
    zmm_write_block(w, &buf[i], sizeof(buf) - i);
}

static inline void format_f64(ZmmWriter* w, double val) {
    if (val < 0) {
        zmm_write_block(w, "-", 1);
        val = -val;
    }

    u64 int_part = (u64)val;
    format_u64(w, int_part);
    zmm_write_block(w, ".", 1);

    char frac_buf[8];
    double frac = val - (double)int_part;
    for (int i = 0; i < 6; i++) {
        frac *= 10.0;
        int digit = (int)frac;
        frac_buf[i] = '0' + digit;
        frac -= digit;
    }
    zmm_write_block(w, frac_buf, 6);
}

// -----------------------------------------------------------------------------
// Core Parser
// -----------------------------------------------------------------------------

static usize zmm_vformat(ZmmWriter* w, const char* format, va_list args) {
    const char* f = format;

    while (*f != '\0') {
        const char* literal_start = f;
        while (*f != '%' && *f != '\0') {
            f++;
        }

        if (f > literal_start) {
            zmm_write_block(w, literal_start, (usize)(f - literal_start));
        }

        if (*f == '\0') break;

        f++;
        if (*f == '%') {
            zmm_write_block(w, "%", 1);
            f++;
            continue;
        }

        // Zig-like Signed Integers
        if (strncmp(f, "i8", 2) == 0) {
            format_i64(w, (i8)va_arg(args, int));
            f += 2;
        } else if (strncmp(f, "i16", 3) == 0) {
            format_i64(w, (i16)va_arg(args, int));
            f += 3;
        } else if (strncmp(f, "i32", 3) == 0) {
            format_i64(w, va_arg(args, i32));
            f += 3;
        } else if (strncmp(f, "i64", 3) == 0) {
            format_i64(w, va_arg(args, i64));
            f += 3;
        }
        // Zig-like Unsigned Integers
        else if (strncmp(f, "u8", 2) == 0) {
            format_u64(w, (u8)va_arg(args, unsigned int));
            f += 2;
        } else if (strncmp(f, "u16", 3) == 0) {
            format_u64(w, (u16)va_arg(args, unsigned int));
            f += 3;
        } else if (strncmp(f, "u32", 3) == 0) {
            format_u64(w, va_arg(args, u32));
            f += 3;
        } else if (strncmp(f, "u64", 3) == 0) {
            format_u64(w, va_arg(args, u64));
            f += 3;
        }
        // Size and Pointer Offsets
        else if (strncmp(f, "usz", 3) == 0) {
            format_u64(w, va_arg(args, usize));
            f += 3;
        } else if (strncmp(f, "ssz", 3) == 0) {
            format_i64(w, va_arg(args, ssize));
            f += 3;
        }
        // Strings
        else if (strncmp(f, "sz", 2) == 0) {
            const char* p = va_arg(args, const char*);
            if (!p) {
                zmm_write_block(w, "(null)", 6);
            } else {
                zmm_write_block(w, p, strlen(p));
            }
            f += 2;
        } else if (strncmp(f, "s", 1) == 0) {
            // Read the struct directly off the vararg list
            SliceCU8 slice = va_arg(args, SliceCU8);

            if (!slice.ptr) {
                zmm_write_block(w, "(null)", 6);
            } else {
                zmm_write_block(w, (const char*)slice.ptr, slice.len);
            }
            f += 1;
        }
        // Basic Printf Specifiers
        else if (strncmp(f, "f", 1) == 0) {
            format_f64(w, va_arg(args, double));
            f += 1;
        } else if (strncmp(f, "c", 1) == 0) {
            char c = (char)va_arg(args, int);
            zmm_write_block(w, &c, 1);
            f += 1;
        } else if (strncmp(f, "d", 1) == 0) {
            format_i64(w, va_arg(args, int));
            f += 1;
        } else if (strncmp(f, "x", 1) == 0) {
            format_hex(w, va_arg(args, unsigned int));
            f += 1;
        } else if (strncmp(f, "p", 1) == 0) {
            void* ptr = va_arg(args, void*);
            zmm_write_block(w, "0x", 2);
            format_hex(w, (u64)(uintptr_t)ptr);
            f += 1;
        } else if (strncmp(f, "b", 1) == 0) {
            bool b = (bool)va_arg(args, int);

            if (b) {
                zmm_write_block(w, "true", 4);
            } else {
                zmm_write_block(w, "false", 5);
            }
            f += 1;
        } else {
            zmm_write_block(w, "%", 1);
            zmm_write_block(w, f, 1);
            f += 1;
        }
    }

    // FINALIZATION

    // Flush any pending data in the stack buffer to stdout
    if (w->stream && w->flush_len > 0) {
        fwrite(w->flush_buf, 1, w->flush_len, w->stream);
        w->flush_len = 0;
    }

    // Safely apply null-terminator ONLY for memory buffers
    if (!w->stream && !w->is_tl && w->buf && w->size > 0) {
        usize null_pos = w->written < w->size ? w->written : w->size - 1;
        w->buf[null_pos] = '\0';
    }

    return w->written;
}

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

usize zmm_snprintf(char* str, usize size, const char* format, ...) {
    ZmmWriter w = {
        .buf = str, .size = size, .written = 0};  // flush_buf remains NULL
    va_list args;
    va_start(args, format);
    usize ret = zmm_vformat(&w, format, args);
    va_end(args);
    return ret;
}

usize zmm_sprintf(char* str, const char* format, ...) {
    ZmmWriter w = {.buf = str, .size = USIZE_MAX, .written = 0};
    va_list args;
    va_start(args, format);
    usize ret = zmm_vformat(&w, format, args);
    va_end(args);
    return ret;
}

usize zmm_printf(const char* format, ...) {
    // Memory efficient: The 1024-byte buffer is safely allocated on the stack
    // ONLY when zmm_printf is executed.
    char stack_buf[1024];
    ZmmWriter w = {.stream = stdout,
                   .flush_buf = stack_buf,
                   .flush_cap = sizeof(stack_buf),
                   .flush_len = 0};
    va_list args;
    va_start(args, format);
    usize ret = zmm_vformat(&w, format, args);
    va_end(args);
    return ret;
}

// Write to the current thread's thread-local accumulator
usize zmm_lprintf(const char* format, ...) {
    ZmmWriter w = {.is_tl = true};  // Engage thread-local logic
    va_list args;
    va_start(args, format);
    usize ret = zmm_vformat(&w, format, args);
    va_end(args);
    return ret;
}

// Safely emit the thread's accumulated logs and reset the accumulator
void zmm_lemit(void) {
    if (tl_len == 0) return;

    // Choose the active buffer (heap if it exceeded 4096, static otherwise)
    const char* data = tl_dyn_buf ? tl_dyn_buf : tl_static_buf;

    // Write thread-safely
    pthread_mutex_lock(&lemit_mutex);
    fwrite(data, 1, tl_len, stdout);
    fflush(stdout);  // Guarantee terminal output
    pthread_mutex_unlock(&lemit_mutex);

    // Reset length. Note: We keep `tl_dyn_buf` (if allocated) alive so that
    // subsequent large logs on this thread do not have to pay the malloc
    // penalty again.
    tl_len = 0;
}
