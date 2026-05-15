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

#pragma once

#include "export.h"
#include "num.h"

/**
 * Custom (optionally thread-safe) printf implementation.
 * 
 * Supported format specifiers:
 * - %i8, %i16, %i32, %i64: Fixed-width signed integers.
 * - %u8, %u16, %u32, %u64: Fixed-width unsigned integers.
 * - %usz: usize
 * - %ssz: ssize
 * - %d: i64 (alias)
 * - %f: double
 * - %.{X}f (e.g., %.2f): Rounded floats
 * - %x: u64 hex
 * - %p: pointer
 * - %c: char
 * - %s: SliceCU8
 * - %sz: null-terminated char*
 * - %b: bool
 */

/**
 * Formats a string into a buffer with a size limit.
 * 
 * @param str The destination buffer.
 * @param size The maximum number of bytes to write (including null terminator).
 * @param format The format string.
 * @param ... Additional arguments for the format string.
 * @return The number of bytes that would have been written (excluding null terminator).
 */
API usize zmm_snprintf(char* str, usize size, const char* format, ...);

/**
 * Formats a string into a buffer.
 * 
 * @param str The destination buffer.
 * @param format The format string.
 * @param ... Additional arguments for the format string.
 * @return The number of bytes written (excluding null terminator).
 */
API usize zmm_sprintf(char* str, const char* format, ...);

/**
 * Prints a formatted string to stdout.
 * 
 * @param format The format string.
 * @param ... Additional arguments for the format string.
 * @return The number of bytes printed.
 */
API usize zmm_printf(const char* format, ...);

/**
 * Prints to a thread-local buffer.
 * 
 * @param format The format string.
 * @param ... Additional arguments for the format string.
 * @return The number of bytes added to the buffer.
 * 
 * [Note] Use zmm_lemit() to emit the buffer thread-safely to stdout.
 */
API usize zmm_lprintf(const char* format, ...);

/**
 * Emits the accumulated thread-local buffer to stdout thread-safely.
 */
API void zmm_lemit();
