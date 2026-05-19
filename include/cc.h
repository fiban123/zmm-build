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

#include "arena.h"
#include "arr.h"
#include "slice.h"

/**
 * A single entry in a compilation database.
 */
typedef struct {
    char* directory;  // Pointer to arena-backed null-terminated string
    SliceCU8 file;
    char* args;  // Packed array of null-terminated strings
    usize num_args;
} CompileCommand;

/**
 * A collection of compilation commands, typically parsed from
 * compile_commands.json.
 */
typedef struct {
    char* current_directory;  // Tracks the currently active directory
    arr(CompileCommand) items;
    pthread_mutex_t lock;
    void* map;
    ArenaAlloc* arena;
} CompileCommands;

/**
 * Parses a compile_commands.json file into the CompileCommands struct.
 *
 * @param cc Pointer to the struct to populate.
 * @param arena Pointer to an arena for backing strings.
 * @param path Path to the JSON file.
 * @return 0 on success, -1 on failure.
 *
 * [Note] Strings are backed by the provided arena. the directory is initialized to the CWD.
 */
API i32 zmm_cc_init_parse(CompileCommands* cc, ArenaAlloc* arena,
                          SliceCU8 path);

/**
 * Appends a compile command to the list.
 *
 * @param cc Pointer to the CompileCommands struct.
 * @param file The source file path.
 * @param args Packed array of null-terminated strings.
 * @param num_args Number of arguments in the packed array.
 * @return 0 on success, -1 on failure.
 */
API i32 zmm_cc_append(CompileCommands* cc, SliceCU8 file, const char* args,
                      usize num_args);

/**
 * Appends a compile command with a specific directory.
 *
 * @param cc Pointer to the CompileCommands struct.
 * @param file The source file path.
 * @param args Packed array of null-terminated strings.
 * @param num_args Number of arguments in the packed array.
 * @param dir The working directory for this command.
 * @return 0 on success, -1 on failure.
 *
 * [Note] Updates the current_directory in CompileCommands. Subsequent appends will use the same directory.
 */
API i32 zmm_cc_append_dir(CompileCommands* cc, SliceCU8 file, const char* args,
                          usize num_args, SliceCU8 dir);

/**
 * Writes the CompileCommands to a file in JSON format.
 *
 * @param cc Pointer to the CompileCommands struct.
 * @param path Path to the output file.
 * @return 0 on success, -1 on failure.
 */
API i32 zmm_cc_write(CompileCommands* cc, SliceCU8 path);

/**
 * Frees the internal array of compile commands.
 *
 * @param cc Pointer to the struct to free.
 *
 * [Note] Does not free the arena-backed strings.
 */
API void zmm_cc_free(CompileCommands* cc);
