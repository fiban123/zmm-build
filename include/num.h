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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Fixed-width signed integer types.
 */
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
#ifdef __SIZEOF_INT128__
typedef __int128_t i128;
#endif

/**
 * Fixed-width unsigned integer types.
 */
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
#ifdef __SIZEOF_INT128__
typedef __uint128_t u128;
#endif

/**
 * Size and pointer-difference types.
 */
typedef size_t usize;
typedef ptrdiff_t ssize;

#define I8_MAX INT8_MAX
#define I16_MAX INT16_MAX
#define I32_MAX INT32_MAX
#define I64_MAX INT64_MAX

#define U8_MAX UINT8_MAX
#define U16_MAX UINT16_MAX
#define U32_MAX UINT32_MAX
#define U64_MAX UINT64_MAX

#define USIZE_MAX SIZE_MAX
#define SSIZE_MAX PTRDIFF_MAX
