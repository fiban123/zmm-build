#pragma once

#include "arena.h"
#include "arr.h"
#include "export.h"
#include "slice.h"

typedef struct {
    char* directory;  // Pointer to arena-backed null-terminated string
    SliceCU8 file;
    char* args;  // Packed array of null-terminated strings
    usize num_args;
} CompileCommand;

typedef struct {
    char* current_directory;  // Tracks the currently active directory
    arr(CompileCommand) items;
} CompileCommands;

/**
 * Parses a compile_commands.json file into the CompileCommands struct.
 * Strings are backed by the provided arena.
 * @return 0 on success, -1 on failure.
 */
API i32 zmm_cc_parse(CompileCommands* cc, ArenaAlloc* arena, SliceCU8 path);

/**
 * Appends a compile command to the list.
 * @param args Packed array of null-terminated strings.
 * @param num_args Number of arguments in the packed array.
 */
API i32 zmm_cc_append(CompileCommands* cc, ArenaAlloc* arena, SliceCU8 file,
                      const char* args, usize num_args);

/**
 * Appends a compile command with a specific directory.
 * Updates the current_directory in CompileCommands;
 */
API i32 zmm_cc_append_dir(CompileCommands* cc, ArenaAlloc* arena, SliceCU8 file,
                          const char* args, usize num_args, SliceCU8 dir);

/**
 * Writes the CompileCommands to a file in JSON format.
 */
API i32 zmm_cc_write(CompileCommands* cc, SliceCU8 path);

/**
 * Frees the internal array of compile commands.
 * NOTE: Does not free the arena-backed strings.
 */
API void zmm_cc_free(CompileCommands* cc);
