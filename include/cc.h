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

#include <pthread.h>
#include <stdio.h>

#include "arena.h"
#include "arr.h"
#include "slice.h"

// --- Writer ---

/**
 * A streaming compile_commands.json writer.
 * Entries are written directly to the file on each append.
 */
typedef struct {
    FILE* f;
    char buf[8192];
    usize buf_len;
    usize count;
    pthread_mutex_t lock;
} CompileCommands;

/**
 * Opens a compile_commands.json file for writing.
 *
 * @param cc Pointer to the struct to initialize.
 * @param path Path to the output file.
 * @return 0 on success, -1 on failure.
 */
API i32 zmm_cc_init(CompileCommands* cc, SliceCU8 path);

/**
 * Appends a compile command entry directly to the file.
 * Thread-safe.
 *
 * @param cc Pointer to the CompileCommands writer.
 * @param dir The working directory for this command.
 * @param file The source file path.
 * @param args Packed array of null-terminated strings.
 * @param num_args Number of arguments in the packed array.
 * @return 0 on success, -1 on failure.
 */
API i32 zmm_cc_append(CompileCommands* cc, SliceCU8 dir, SliceCU8 file,
                      const char* args, usize num_args);

/**
 * Finishes writing and closes the file.
 *
 * @param cc Pointer to the CompileCommands writer.
 * @return 0 on success, -1 on failure.
 */
API i32 zmm_cc_finish(CompileCommands* cc);

// --- Parser ---

/**
 * A single entry parsed from a compilation database.
 */
typedef struct {
    SliceCU8 directory;
    SliceCU8 file;
    char* args;  // Packed array of null-terminated strings
    usize num_args;
} CompileCommand;

/**
 * Parses a compile_commands.json file.
 *
 * @param out Pointer to an stb_ds array of CompileCommand to populate.
 * @param arena Pointer to an arena for backing strings.
 * @param path Path to the JSON file.
 * @return 0 on success, -1 on failure.
 *
 * [FREE] The array must be freed with arrfree(). Strings are arena-backed.
 */
API i32 zmm_cc_parse(arr(CompileCommand)* out, ArenaAlloc* arena,
                     SliceCU8 path);
