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

#pragma once

#include "export.h"
#include "num.h"
#include "str.h"

/**
 * Custom (optionally thread-safe) printf implementation.
 * * Supported format specifiers:
 * - %i8, %i16, %i32, %i64
 * - %u8, %u16, %u32, %u64
 * - %usz: usize
 * - %ssz: ssize
 * - %d: i64 (alias)
 * - %f: double
 * - %.{X}f (e.g., %.2f): Rounded floats
 * - %x: u64 hex
 * - %p: pointer
 * - %c: char
 * - %s: StringView
 * - %sz: null-terminated char*
 * - %b: bool
 */

API StringView zmm_snprintf(String str, const char* format, ...);

API StringView zmm_sprintf(String str, const char* format, ...);

API usize zmm_printf(const char* format, ...);

API usize zmm_lprintf(const char* format, ...);

API void zmm_lemit();
