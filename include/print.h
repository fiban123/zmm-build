#include "export.h"
#include "num.h"

/**
 * Custom thread-safe printf implementation.
 * Supported format specifiers:
 * %i8, %i16, %i32, %i64, %u8, %u16, %u32, %u64
 * %usz (usize), %ssz (ssize)
 * %d (i64), %f (double), %x (u64 hex), %p (pointer)
 * %c (char), %s (SliceCU8), %sz (null-terminated char*)
 * %b (bool)
 */

/**
 * Formats a string into a buffer with a size limit.
 */
API usize zmm_snprintf(char* str, usize size, const char* format, ...);

/**
 * Formats a string into a buffer.
 */
API usize zmm_sprintf(char* str, const char* format, ...);

/**
 * Prints a formatted string to stdout.
 */
API usize zmm_printf(const char* format, ...);

/**
 * Prints to a thread-local buffer.
 * Use zmm_lemit() to output the buffer thread-safely.
 */
API usize zmm_lprintf(const char* format, ...);

/**
 * Emits the accumulated thread-local buffer to stdout thread-safely.
 */
API void zmm_lemit();
