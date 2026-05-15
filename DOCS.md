# zmm API Documentation

## arena.h

### `ArenaAlloc`

An arena allocator for efficient memory management.
Memory is allocated in large blocks (nodes) and handed out in smaller
pieces. All memory in the arena is freed at once.

### `zmm_arena_init`

```c
void zmm_arena_init(ArenaAlloc* arena);
```

Initializes the arena allocator.

**Parameters:**

- `arena`: Pointer to the arena to initialize.

### `zmm_arena_alloc`

```c
void* zmm_arena_alloc(ArenaAlloc* arena, usize size);
```

Allocates a block of memory from the arena.

**Parameters:**

- `arena`: Pointer to the arena.
- `size`: Number of bytes to allocate.

**Returns:** Pointer to the allocated memory.

**Notes:**

- The memory is not guaranteed to be zero-initialized.

### `zmm_arena_alloc_aligned`

```c
void* zmm_arena_alloc_aligned(ArenaAlloc* arena, usize size);
```

Allocates an 8-byte aligned block of memory from the arena.

**Parameters:**

- `arena`: Pointer to the arena.
- `size`: Number of bytes to allocate.

**Returns:** Pointer to the allocated memory.

**Notes:**

- Useful for ints or other types that require alignment.

### `zmm_arena_dupe`

```c
void* zmm_arena_dupe(ArenaAlloc* arena, const void* mem, usize n);
```

Duplicates a memory block into the arena.

**Parameters:**

- `arena`: Pointer to the arena.
- `mem`: Pointer to the memory to duplicate.
- `n`: Number of bytes to duplicate.

**Returns:** Pointer to the new memory block in the arena.

### `zmm_arena_free`

```c
void zmm_arena_free(ArenaAlloc* arena);
```

Frees all memory associated with the arena and its nodes.

**Parameters:**

- `arena`: Pointer to the arena to free.

## args.h

### `ArgvBuilder`

A builder for constructing command-line arguments.
It is more efficient than fragmented argument dynamic arrays. Additionally,
it always keeps track of pointers to the arguments, which means that
executing the command requires no extra allocations.

### `zmm_argv_initbuf`

```c
void zmm_argv_initbuf(ArgvBuilder* cmd, void* flat_buf, usize flat_buf_size, void* argv_buf, usize argv_buf_size);
```

Initializes the ArgvBuilder struct with provided buffers.
The builder uses these buffers first and falls back to heap if they are
filled.

**Parameters:**

- `cmd`: Pointer to the builder to initialize.
- `flat_buf`: Stack buffer for argument strings.
- `flat_buf_size`: Size of flat_buf.
- `argv_buf`: Stack buffer for argument pointers.
- `argv_buf_size`: Size of argv_buf.

**Cleanup:**

- [FREE] The resulting struct must be freed with `zmm_argv_free()`.

**Notes:**

- Useful for bypassing heap allocations with stack buffers.

### `zmm_argv_initcap`

```c
void zmm_argv_initcap(ArgvBuilder* cmd, usize flat_cap, usize argv_cap);
```

Initializes the ArgvBuilder struct with initial capacities allocated on the heap.

**Parameters:**

- `cmd`: Pointer to the builder to initialize.
- `flat_cap`: Initial capacity for argument strings.
- `argv_cap`: Initial capacity for argument pointers.

**Cleanup:**

- [FREE] The resulting struct must be freed with `zmm_argv_free()`.

### `zmm_argv_append`

```c
void zmm_argv_append(ArgvBuilder* argv, SliceCU8 arg);
```

Appends an argument to the command.

**Parameters:**

- `argv`: Pointer to the builder.
- `arg`: The argument slice to append.

### `zmm_argv_append_argv`

```c
void zmm_argv_append_argv(ArgvBuilder* argv, const ArgvBuilder* src);
```

Appends all arguments from an existing command (ArgvBuilder) to the current
command.

**Parameters:**

- `argv`: Pointer to the destination builder.
- `src`: Pointer to the source builder.

### `zmm_argv_appendz`

```c
void zmm_argv_appendz(ArgvBuilder* argv, const SliceCU8* args);
```

Appends a list of arguments to the command.

**Parameters:**

- `argv`: Pointer to the builder.
- `args`: Nullslice-terminated array of SliceCU8.

### `zmm_argv_pappend`

```c
void zmm_argv_pappend(ArgvBuilder* argv, SliceCU8 arg);
```

Appends part of an argument (partial append).

**Parameters:**

- `argv`: Pointer to the builder.
- `arg`: The argument part to append.

### `zmm_argv_pfinish`

```c
void zmm_argv_pfinish(ArgvBuilder* argv);
```

Finishes the current partial argument.

**Parameters:**

- `argv`: Pointer to the builder.

### `zmm_argv_free`

```c
void zmm_argv_free(ArgvBuilder* argv);
```

Frees the command builder's buffers.

**Parameters:**

- `argv`: Pointer to the builder to free.

### `Argv`

A structure representing parsed command-line arguments as slices.

### `zmm_args_init`

```c
i32 zmm_args_init(Argv* args, int argc, char** argv);
```

Initializes the Argv struct from standard C argc/argv.

**Parameters:**

- `args`: Pointer to the Argv struct to initialize.
- `argc`: Argument count.
- `argv`: Argument vector.

**Returns:** 0 on success, -1 on failure.

**Cleanup:**

- [FREE] The resulting struct must be freed with `zmm_args_free()`.

### `zmm_args_free`

```c
void zmm_args_free(Argv* argv);
```

Frees the Argv struct.

**Parameters:**

- `argv`: Pointer to the Argv struct to free.

### `zmm_args_contains`

```c
bool zmm_args_contains(const Argv* argv, SliceCU8 flag);
```

Returns whether a flag is present in the arguments.

**Parameters:**

- `argv`: Pointer to the arguments.
- `flag`: The flag to search for.

**Returns:** true if the flag is present, false otherwise.

### `zmm_args_get_value`

```c
SliceCU8 zmm_args_get_value(const Argv* argv, SliceCU8 key);
```

Returns the value of a conjoined argument like `--depth=100`.

**Parameters:**

- `argv`: Pointer to the arguments.
- `key`: The key to search for (including the equal sign, e.g., "--port=").

**Returns:** Slice containing the value, or a nullslice if not found.

### `zmm_args_get_subsequent`

```c
SliceCU8 zmm_args_get_subsequent(const Argv* argv, SliceCU8 key);
```

Returns the value of a separated argument like `--port 8080`.

**Parameters:**

- `argv`: Pointer to the arguments.
- `key`: The key to search for.

**Returns:** Slice containing the value, or a nullslice if not found.

## arr.h

This exists purely to explicitly denote what is a dynamic array
and what is a regular pointer.

### `arr(T)`

Declares a dynamic array of type T.

**Notes:**

- Uses stb_ds.h under the hood.

### `arrinit`

Initializer for dynamic arrays.

## builder.h

### `BuilderFn`

```c
typedef int (*BuilderFn)(const SliceCU8* sources, usize num_sources, SliceCU8 output, u32 thread_idx);
```

The builder callback that executes a build task.

**Parameters:**

- `sources`: Array of source file paths.
- `num_sources`: Number of source files.
- `output`: Path to the output file.
- `thread_idx`: Index of the thread executing the task.

**Returns:** 0 on success, non-zero to abort the build.

**Notes:**

- Typically calls a compiler or linker via zmm_sys_exec_print.

### `BuildGraph`

A build graph (DAG) representing targets and their dependencies. Uses
an arena for nearly all internal allocations.

### `zmm_bg_init`

```c
void zmm_bg_init(BuildGraph* g, ArenaAlloc* arena);
```

Initializes the build graph.

**Parameters:**

- `g`: Pointer to the graph to initialize.
- `arena`: Pointer to an arena for internal allocations.

### `zmm_bg_free`

```c
void zmm_bg_free(BuildGraph* g);
```

Frees memory associated with the buildgraph. Does not free the arena.

**Parameters:**

- `g`: Pointer to the graph to free.

### `zmm_bg_add`

```c
void zmm_bg_add(BuildGraph* g, SliceCU8 const* sources, usize num_sources, SliceCU8 output, SliceCU8 const* deps, usize num_deps);
```

Adds a target to the DAG.

**Parameters:**

- `g`: Pointer to the graph.
- `sources`: Array of source files.
- `num_sources`: Number of source files.
- `output`: The output target file.
- `deps`: Optional array of extra dependencies.
- `num_deps`: Number of extra dependencies.

**Notes:**

- All inputs are copied into the arena to ensure they persist. To avoid this,
see [TargetBuilder]

### `zmm_bg_build`

```c
int zmm_bg_build(BuildGraph* g, const SliceCU8* targets, usize num_targets, BuilderFn builder);
```

Executes the build for an array of targets, isolating its subgraph. It calls
the builder function for any target which needs to be built.

**Parameters:**

- `g`: Pointer to the graph.
- `target`: The target to build.
- `builder`: The function used to build each target.

**Returns:** 0 on success, -1 if a task failed or target wasn't found.

**Notes:**

- Runs tasks in parallel using a thread pool.

### `zmm_bg_is_dirty`

```c
bool zmm_bg_is_dirty(SliceCU8 a_path, SliceCU8 b_path);
```

Returns true if the `a` needs to be rebuilt, based on `b`.

**Parameters:**

- `a_path`: The output file path.
- `b_path`: The input file path.

**Returns:** true if b_path is newer than a_path, or either does not exist.

**Notes:**

- Useful for simple checks without a full build graph.

### `TargetBuilder`

A lightweight handle to a target being constructed. Allows for
much more complex behaviour than a simple zmm_bg_add call. There
are also non-copying versions.

### `zmm_tg_init_out`

```c
void zmm_tg_init_out(TargetBuilder* tg, BuildGraph* bg, SliceCU8 output);
```

Initializes a target builder for a specific output.

**Parameters:**

- `tg`: Pointer to the target builder.
- `bg`: Pointer to the build graph.
- `output`: The output target file.

### `zmm_tg_add_src`

```c
void zmm_tg_add_src(TargetBuilder* tb, SliceCU8 const* sources, usize count);
```

Adds sources to the target being built.

**Parameters:**

- `tb`: Pointer to the target builder.
- `sources`: Array of source files.
- `count`: Number of source files.

### `zmm_tg_add_src_nc`

```c
void zmm_tg_add_src_nc(TargetBuilder* tb, SliceCU8 const* sources, usize count);
```

Adds sources to the target without copying them.

**Parameters:**

- `tb`: Pointer to the target builder.
- `sources`: Array of source files.
- `count`: Number of source files.

**Notes:**

- Sources must persist in memory.

### `zmm_tg_add_dep`

```c
void zmm_tg_add_dep(TargetBuilder* tb, SliceCU8 const* deps, usize count);
```

Adds extra dependencies to the target.

**Parameters:**

- `tb`: Pointer to the target builder.
- `deps`: Array of dependency files.
- `count`: Number of dependency files.

### `zmm_tg_add_dep_nc`

```c
void zmm_tg_add_dep_nc(TargetBuilder* tb, SliceCU8 const* deps, usize count);
```

Adds extra dependencies to the target without copying them.

**Parameters:**

- `tb`: Pointer to the target builder.
- `deps`: Array of dependency files.
- `count`: Number of dependency files.

**Notes:**

- Deps must persist in memory.

## cc.h

### `CompileCommand`

A single entry in a compilation database.

### `CompileCommands`

A collection of compilation commands, typically parsed from
compile_commands.json.

### `zmm_cc_parse`

```c
i32 zmm_cc_init_parse(CompileCommands* cc, ArenaAlloc* arena, SliceCU8 path);
```

Parses a compile_commands.json file into the CompileCommands struct.

**Parameters:**

- `cc`: Pointer to the struct to populate.
- `arena`: Pointer to an arena for backing strings.
- `path`: Path to the JSON file.

**Returns:** 0 on success, -1 on failure.

**Notes:**

- Strings are backed by the provided arena. the directory is initialized to the CWD.

### `zmm_cc_append`

```c
i32 zmm_cc_append(CompileCommands* cc, SliceCU8 file, const char* args, usize num_args);
```

Appends a compile command to the list.

**Parameters:**

- `cc`: Pointer to the CompileCommands struct.
- `file`: The source file path.
- `args`: Packed array of null-terminated strings.
- `num_args`: Number of arguments in the packed array.

**Returns:** 0 on success, -1 on failure.

### `zmm_cc_append_dir`

```c
i32 zmm_cc_append_dir(CompileCommands* cc, SliceCU8 file, const char* args, usize num_args, SliceCU8 dir);
```

Appends a compile command with a specific directory.

**Parameters:**

- `cc`: Pointer to the CompileCommands struct.
- `file`: The source file path.
- `args`: Packed array of null-terminated strings.
- `num_args`: Number of arguments in the packed array.
- `dir`: The working directory for this command.

**Returns:** 0 on success, -1 on failure.

**Notes:**

- Updates the current_directory in CompileCommands. Subsequent appends will use the same directory.

### `zmm_cc_write`

```c
i32 zmm_cc_write(CompileCommands* cc, SliceCU8 path);
```

Writes the CompileCommands to a file in JSON format.

**Parameters:**

- `cc`: Pointer to the CompileCommands struct.
- `path`: Path to the output file.

**Returns:** 0 on success, -1 on failure.

### `zmm_cc_free`

```c
void zmm_cc_free(CompileCommands* cc);
```

Frees the internal array of compile commands.

**Parameters:**

- `cc`: Pointer to the struct to free.

**Notes:**

- Does not free the arena-backed strings.

## cpu.h

### `CpuInfo`

A structure containing CPU feature and cache information.
It wraps google/cpu_features structures.

### `zmm_cpu_init`

```c
void zmm_cpu_init(CpuInfo* info);
```

Populates the CpuInfo struct with x86 features and cache information.

**Parameters:**

- `info`: Pointer to the struct to populate.

### `zmm_cpu_supports(info_ptr, feat)`

Returns whether a specific CPU feature is supported.

**Parameters:**

- `info_ptr`: Pointer to a CpuInfo struct.
- `feat`: The feature to check (e.g., avx2, sse4_1).

### `zmm_cpu_cache_size`

```c
i32 zmm_cpu_cache_size(const CpuInfo* info, int level, CacheType cache_type);
```

Returns the cache size in bytes for a given level and type.

**Parameters:**

- `info`: Pointer to the CpuInfo struct.
- `level`: Cache level (1, 2, 3).
- `cache_type`: Type of cache (DATA, INSTRUCTION, UNIFIED).

**Returns:** Cache size in bytes, or 0 if missing.

### `zmm_cpu_l1d_size`

```c
i32 zmm_cpu_l1d_size(const CpuInfo* info);
```

Returns L1 Data Cache size in bytes.

**Parameters:**

- `info`: Pointer to the CpuInfo struct.

**Returns:** Size in bytes.

### `zmm_cpu_l1i_size`

```c
i32 zmm_cpu_l1i_size(const CpuInfo* info);
```

Returns L1 Instruction Cache size in bytes.

**Parameters:**

- `info`: Pointer to the CpuInfo struct.

**Returns:** Size in bytes.

### `zmm_cpu_l2_size`

```c
i32 zmm_cpu_l2_size(const CpuInfo* info);
```

Returns L2 Unified (or Data) Cache size in bytes.

**Parameters:**

- `info`: Pointer to the CpuInfo struct.

**Returns:** Size in bytes.

### `zmm_cpu_l3_size`

```c
i32 zmm_cpu_l3_size(const CpuInfo* info);
```

Returns L3 Unified Cache size in bytes.

**Parameters:**

- `info`: Pointer to the CpuInfo struct.

**Returns:** Size in bytes.

### `zmm_cpu_thread_count`

```c
i32 zmm_cpu_thread_count();
```

Returns the number of online logical processors.

**Returns:** Logical processor count.

### `zmm_cpu_brand_string`

```c
SliceU8 zmm_cpu_brand_string(const CpuInfo* info);
```

Returns the full CPU brand string.

**Parameters:**

- `info`: Pointer to the CpuInfo struct.

**Returns:** Slice containing the brand string.

**Cleanup:**

- [FREE] The resulting slice's ptr must be freed.

### `zmm_cpu_vendor`

```c
SliceU8 zmm_cpu_vendor(const CpuInfo* info);
```

Returns the CPU Vendor ID string.

**Parameters:**

- `info`: Pointer to the CpuInfo struct.

**Returns:** Slice containing the vendor string.

**Cleanup:**

- [FREE] The resulting slice's ptr must be freed.

### `zmm_cpu_uarch_str`

```c
SliceU8 zmm_cpu_uarch_str(const CpuInfo* info);
```

Returns a string representation of the Microarchitecture.

**Parameters:**

- `info`: Pointer to the CpuInfo struct.

**Returns:** Slice containing the uarch string.

**Cleanup:**

- [FREE] The resulting slice's ptr must be freed.

### `zmm_cpu_family`

```c
i32 zmm_cpu_family(const CpuInfo* info);
```

Returns the CPU family ID.

**Parameters:**

- `info`: Pointer to the CpuInfo struct.

**Returns:** Family ID.

### `zmm_cpu_model`

```c
i32 zmm_cpu_model(const CpuInfo* info);
```

Returns the CPU model ID.

**Parameters:**

- `info`: Pointer to the CpuInfo struct.

**Returns:** Model ID.

### `zmm_cpu_stepping`

```c
i32 zmm_cpu_stepping(const CpuInfo* info);
```

Returns the CPU stepping ID.

**Parameters:**

- `info`: Pointer to the CpuInfo struct.

**Returns:** Stepping ID.

## dep.h

### `zmm_dep_parse`

```c
i32 zmm_dep_parse(arr(SliceU8) * deps, SliceCU8 path);
```

Parses a compiler dependency file (.d) and appends dependencies
as slices to an existing array.

**Parameters:**

- `deps`: Pointer to an stb_ds array of SliceU8. Can point to NULL initially.
- `path`: Path to the dependency file.

**Returns:** 0 on success, -1 if the file could not be read.

## fs.h

### `FindDepth`

Specifies the depth of a filesystem search.

- `FD_SHALLOW`: Only search the specified directories.
- `FD_RECURSIVE`: Search specified directories and all subdirectories.

### `FindKind`

Specifies the kind of filesystem entry to find.

- `FK_FILE`: Find files.
- `FK_DIRECTORY`: Find directories.

NOTE: more filsystem entry types such as symlinks exist and may be added in a future version.

### `FindOpts`

Options for configuring a filesystem search via zmm_fs_find.

### `zmm_fs_find`

```c
i32 zmm_fs_find(arr(SliceU8) * out, const SliceCU8* roots, FindOpts opts);
```

Finds all matches given the roots and find options, appends them to out.

**Parameters:**

- `out`: Pointer to an stb_ds array of SliceU8.
- `roots`: Nullslice-terminated array of root directories to search in.
- `opts`: Search options (depth, kinds, extensions, hidden).

**Returns:** 0 on success, otherwise -1.

**Cleanup:**

- [FREE] The results appended to 'out' must be freed with `slicearr_free()`.

### `zmm_fs_create_dir_path`

```c
i32 zmm_fs_create_dir_path(SliceCU8 path);
```

Creates a directory and all its parents (equivalent to `mkdir -p`).

**Parameters:**

- `path`: The directory path to create.

**Returns:** 0 on success, otherwise -1.

### `zmm_fs_create_parent_path`

```c
i32 zmm_fs_create_parent_path(SliceCU8 path);
```

Creates the parent directory of a file path.

**Parameters:**

- `path`: The file path whose parent directory should be created.

**Returns:** 0 on success, otherwise -1.

### `zmm_fs_file_exists`

```c
bool zmm_fs_file_exists(SliceCU8 path);
```

Returns whether a file or directory exists.

**Parameters:**

- `path`: The path to check.

**Returns:** true if it exists, false otherwise.

### `zmm_fs_delete_file`

```c
i32 zmm_fs_delete_file(SliceCU8 path);
```

Deletes a file.

**Parameters:**

- `path`: The path to the file to delete.

**Returns:** 0 on success, otherwise -1.

### `zmm_fs_delete_dir`

```c
i32 zmm_fs_delete_dir(SliceCU8 path);
```

Deletes an empty directory.

**Parameters:**

- `path`: The path to the directory to delete.

**Returns:** 0 on success, otherwise -1.

### `zmm_fs_delete_tree`

```c
i32 zmm_fs_delete_tree(SliceCU8 path);
```

Recursively deletes a directory tree.

**Parameters:**

- `path`: The path to the directory tree to delete.

**Returns:** 0 on success, otherwise -1.

### `zmm_fs_abs_cwd`

```c
SliceU8 zmm_fs_abs_cwd(void);
```

Returns the absolute path of the current working directory.

**Returns:** Slice containing the absolute CWD path or a nullslice on a failure.

**Cleanup:**

- [FREE] The resulting slice's ptr must be freed.

### `zmm_fs_change_cwd`

```c
i32 zmm_fs_change_cwd(SliceCU8 path);
```

Changes the current working directory.

**Parameters:**

- `path`: The new working directory path.

**Returns:** 0 on success, otherwise -1.

## num.h

### `i8`, `i16`, `i32`, `i64`, `i128`

Fixed-width signed integer types.

### `u8`, `u16`, `u32`, `u64`, `u128`

Fixed-width unsigned integer types.

### `usize`, `ssize`

Size and pointer-difference types.

## path.h

### `zmm_p_ext`

```c
SliceCU8 zmm_p_ext(SliceCU8 path);
```

Returns the extension of `path` as a slice.

**Parameters:**

- `path`: The path to extract the extension from.

**Returns:** Slice containing the extension, or a nullslice if none.

### `zmm_p_stem`

```c
SliceCU8 zmm_p_stem(SliceCU8 path);
```

Returns the stem (filename without extension) of `path`.

**Parameters:**

- `path`: The path to extract the stem from.

**Returns:** Slice containing the stem.

### `zmm_p_strip_ext`

```c
SliceCU8 zmm_p_strip_ext(SliceCU8 path);
```

Returns the entire path with the extension removed.

**Parameters:**

- `path`: The path to strip the extension from.

**Returns:** Slice containing the path without extension.

**Notes:**

- Example: src/test/main.c -> src/test/main

### `zmm_p_has_ext`

```c
bool zmm_p_has_ext(SliceCU8 path, SliceCU8 ext);
```

Returns whether the path has the specified extension.

**Parameters:**

- `path`: The path to check.
- `ext`: The extension to look for.

**Returns:** true if the path has the extension, false otherwise.

### `zmm_p_has_exts`

```c
bool zmm_p_has_exts(SliceCU8 path, const SliceCU8* exts);
```

Returns whether the path has any of the specified extensions.

**Parameters:**

- `path`: The path to check.
- `exts`: Nullslice-terminated array of extensions.

**Returns:** true if the path has any of the extensions, false otherwise.

### `zmm_p_is_hidden`

```c
bool zmm_p_is_hidden(SliceCU8 path);
```

Returns whether the file is hidden (starts with a dot).

**Parameters:**

- `path`: The path to check.

**Returns:** true if it is hidden, false otherwise.

### `zmm_p_join_any`

```c
SliceU8 zmm_p_join_any(const SliceCU8* parts, u8* stack_buf, usize stack_buf_size, u8** heap_out);
```

Joins multiple path parts together into a single path.

**Parameters:**

- `parts`: Nullslice-terminated array of path parts.
- `stack_buf`: Optional stack buffer to avoid allocation.
- `stack_buf_size`: Size of the stack buffer.
- `heap_out`: Pointer to store the heap-allocated buffer if used.

**Returns:** A slice pointing to the joined path.

**Cleanup:**

- [FREE] The resulting *heap_out must be freed if it is not NULL.

**Notes:**

- if a part ends with a `/`, it is interpreted as a directory
- Otherwise it is interpreted as a file
- It correctly handles directory delimeters and strips redundant "./" from paths.

### `zmm_p_trim_dot_slash`

```c
SliceCU8 zmm_p_trim_dot_slash(SliceCU8 path);
```

Trims leading "./" from a path.

**Parameters:**

- `path`: The path to trim.

**Returns:** Slice containing the trimmed path.

### `zmm_p_is_separator`

```c
bool zmm_p_is_separator(char c);
```

Returns whether a character is a path separator.

**Parameters:**

- `c`: The character to check.

**Returns:** true if it is a separator, false otherwise.

### `zmm_p_normalize_sep`

```c
void zmm_p_normalize_sep(SliceU8 path);
```

Normalizes all separators in a path to forward slashes.

**Parameters:**

- `path`: The path to normalize (modified in-place).

## print.h

Custom (optionally thread-safe) printf implementation.

Supported format specifiers:

- `%i8`, `%i16`, `%i32`, `%i64`: Fixed-width signed integers.
- `%u8`, `%u16`, `%u32`, `%u64`: Fixed-width unsigned integers.
- `%usz`: `usize`
- `%ssz`: `ssize`
- `%d`: `i64` (alias)
- `%f`: `double`
- `%.{X}f` (e.g., `%.2f`): Rounded floats
- `%x`: `u64` hex
- `%p`: pointer
- `%c`: `char`
- `%s`: `SliceCU8`
- `%sz`: null-terminated `char*`
- `%b`: `bool`

### `zmm_snprintf`

```c
usize zmm_snprintf(char* str, usize size, const char* format, ...);
```

Formats a string into a buffer with a size limit.

**Parameters:**

- `str`: The destination buffer.
- `size`: The maximum number of bytes to write (including null terminator).
- `format`: The format string.
- `...`: Additional arguments for the format string.

**Returns:** The number of bytes that would have been written (excluding null terminator).

### `zmm_sprintf`

```c
usize zmm_sprintf(char* str, const char* format, ...);
```

Formats a string into a buffer.

**Parameters:**

- `str`: The destination buffer.
- `format`: The format string.
- `...`: Additional arguments for the format string.

**Returns:** The number of bytes written (excluding null terminator).

### `zmm_printf`

```c
usize zmm_printf(const char* format, ...);
```

Prints a formatted string to stdout.

**Parameters:**

- `format`: The format string.
- `...`: Additional arguments for the format string.

**Returns:** The number of bytes printed.

### `zmm_lprintf`

```c
usize zmm_lprintf(const char* format, ...);
```

Prints to a thread-local buffer.

**Parameters:**

- `format`: The format string.
- `...`: Additional arguments for the format string.

**Returns:** The number of bytes added to the buffer.

**Notes:**

- Use zmm_lemit() to emit the buffer thread-safely to stdout.

### `zmm_lemit`

```c
void zmm_lemit();
```

Emits the accumulated thread-local buffer to stdout thread-safely.

## slice.h

Provides many useful macros for slices. These are mainly used
for length-based string handling. A Nullslice is any slice
where `.ptr` is `NULL`.

### `Slice(T)`

Declares a slice of type T.
A slice is a view into a contiguous array of elements.

### `SliceU8`

Typedef for a slice containing `u8`. Used for mutable strings.

### `SliceCU8`

Typedef for a slice containing `const u8`. Used for immutable strings.

### `NullSliceU8`

Macro for a nullslice of type SliceU8

### `NullSliceCU8`

Macro for a nullslice of type SliceCU8

### `strlit(S)`

Creates a SliceCU8 from a string literal.

**Notes:**

- This only works with string literals, not variables.

### `slice_to_cstr(s)`

Converts a Slice to a null-terminated C-string.

**Parameters:**

- `s`: The slice to convert.

**Returns:** A null-terminated C-string.

**Cleanup:**

- [FREE] The resulting pointer MUST be freed with `free()`.

### `slicelit(Typedef, T, ...)`

Creates a slice literal from values.

**Parameters:**

- `Typedef`: The slice type (e.g., SliceCU8).
- `T`: The element type.
- `...`: The values to include in the slice.

### `slicearr(Typedef, ...)`

Creates an array of slices.

### `slice_cast(ToType, s)`

Casts between slice types.

### `slice_eq(a, b)`

Returns whether two slices are equal.

### `slicearr_free(arr)`

Frees an stb_ds array of slices and each slice.

**Parameters:**

- `arr`: The stb_ds array to free.

## sys.h

### `TermType`

Enum of ways a child process can terminate.

- `TERM_EXITED`: The process exited normally with a return code.
- `TERM_SIGNALED`: The process was forcibly terminated by the OS.
- `TERM_ERROR`: The process failed to even start.

### `ChildTerm`

```c
typedef struct {
    TermType type;
    i32 code;
} ChildTerm;
```

Information about a terminated child process.

- `type`: How the child process terminated
- `code`: The exit code of the process (only valid if type is `TERM_EXITED`).

### `ExecResult`

```c
typedef struct {
    ChildTerm status;
    SliceU8 output;
} ExecResult;
```

Bundles the execution status with the captured output buffer.

### `zmm_sys_exec`

```c
ExecResult zmm_sys_exec(char* const* argv, usize num_args);
```

Executes a command and captures its output.
It merges stderr into stdout.

**Parameters:**

- `argv`: Null-terminated array of argument strings.
- `num_args`: Number of arguments.

**Returns:** ExecResult containing the output and status.

**Cleanup:**

- [FREE] The caller must free the output on the returned ExecResult.

### `zmm_sys_exec_print`

```c
ChildTerm zmm_sys_exec_print(char* const* argv, usize num_args);
```

Executes a command and prints its output thread-safely.

**Parameters:**

- `argv`: Null-terminated array of argument strings.
- `num_args`: Number of arguments.

**Returns:** The execution status.

### `zmm_sys_exec_redirect`

```c
ChildTerm zmm_sys_exec_redirect(char* const* argv, usize num_args);
```

Executes a command without capturing output.
The child process inherits stdout/stderr from the parent.

**Parameters:**

- `argv`: Null-terminated array of argument strings.
- `num_args`: Number of arguments.

**Returns:** The execution status.

### `zmm_sys_exec_flat`

```c
ExecResult zmm_sys_exec_flat(const char* arg_buf, usize num_args);
```

Executes a command from a packed argument buffer and captures output.

**Parameters:**

- `arg_buf`: Packed array of null-terminated strings.
- `num_args`: Number of arguments.

**Returns:** ExecResult containing the output and status.

**Cleanup:**

- [FREE] The caller must free the output on the returned ExecResult.

### `zmm_sys_exec_print_flat`

```c
ChildTerm zmm_sys_exec_print_flat(const char* arg_buf, usize num_args);
```

Executes a command from a packed argument buffer and prints output.

**Parameters:**

- `arg_buf`: Packed array of null-terminated strings.
- `num_args`: Number of arguments.

**Returns:** The execution status.

### `zmm_sys_exec_redirect_flat`

```c
ChildTerm zmm_sys_exec_redirect_flat(const char* arg_buf, usize num_args);
```

Executes a command from a packed argument buffer without capturing output.

**Parameters:**

- `arg_buf`: Packed array of null-terminated strings.
- `num_args`: Number of arguments.

**Returns:** The execution status.

## ztime.h

Provides timestamp measuring functions. Useful for measuring compilation time.

### `zmm_time_ms`

```c
u64 zmm_time_ms();
```

Returns the current time in milliseconds.

### `zmm_time_us`

```c
u64 zmm_time_us();
```

Returns the current time in microseconds.

### `zmm_time_ns`

```c
u64 zmm_time_ns();
```

Returns the current time in nanoseconds.

# zmm CLI

zmm CLI will look for a `.zmm` config file, rebuild the build script if it has changed and run the build
script executable with the same args zmm was invoked with.

Example: running `zmm -ffast` will ensure the build script executable is up-to-date, and then execute
it with given args. If your build script executable is named `build.out`,
this would be `./build.out -ffast`.

## Config file

 [NOTE]: The config file must be named `.zmm` and present in the CWD.

Example config file:

```json
{
    "build-script-srcs": [
        "./build.c"
    ],
    "build-script-exe": "./build.out",
    "build-script-compile-cmd": "clang -Og ./build.c -o ./build.out -lzmm"
}
```

- `build-script-srcs`: These are the paths to the build script source files. They are used purely
so zmm knows when the executable needs to be rebuilt. This is almost always just a single file.
Optionally, you can also include the `.zmm` config file itself to rebuild when the config updates.
- `build-script-exe`: This is the path to the executable that zmm will invoke
- `build-script-compile-cmd`: This is the command zmm runs if the executable needs to be rebuilt.
