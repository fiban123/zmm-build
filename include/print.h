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
