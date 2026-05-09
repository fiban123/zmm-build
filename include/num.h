#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * Fixed-width signed integer types.
 */
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef __int128_t i128;

/**
 * Fixed-width unsigned integer types.
 */
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef __uint128_t u128;

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
#define SSIZE_MAX SSIZE_MAX
