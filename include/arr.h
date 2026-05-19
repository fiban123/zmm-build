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

/**
 * This exists purely to explicitly denote what is a dynamic array
 * and what is a regular pointer.
 */

/**
 * Declares a dynamic array of type T.
 * 
 * [Note] Uses stb_ds.h under the hood.
 */
#define arr(T) T*

/**
 * Initializer for dynamic arrays.
 */
#define arrinit NULL

/**
 * Declares a string hash map with values of type T.
 * 
 * [Note] Uses stb_ds.h under the hood.
 */
#define stringhm(T) T*

/**
 * Initializer for string hash maps.
 */
#define stringhm_init NULL
