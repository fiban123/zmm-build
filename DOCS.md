# zmm API Documentation

## arena.h

### `zmm_arena_init`

```c
void zmm_arena_init(ArenaAlloc* arena);
```

Initializes the arena allocator.

### `zmm_arena_alloc`

```c
void* zmm_arena_alloc(ArenaAlloc* arena, usize size);
```

Allocates a block of memory from the arena. The memory is not guaranteed to be zero-initialized.

### `zmm_arena_alloc_aligned`

```c
void* zmm_arena_alloc_aligned(ArenaAlloc* arena, usize size);
```

Allocates an 8-byte aligned block of memory from the arena. Useful for integers.

### `zmm_arena_dupe`

```c
void* zmm_arena_dupe(ArenaAlloc* arena, const void* mem, usize n);
```

Duplicates a memory block into the arena.

### `zmm_arena_free`

```c
void zmm_arena_free(ArenaAlloc* arena);
```

Frees all memory associated with the arena.

## args.h

```c
typedef struct {
    char* args;
    usize args_cap;
    usize args_len;
    bool args_is_heap;

    char** argptr_buf;
    usize argptr_cap;
    usize num_args;
    bool argptrs_is_heap;
} ArgvBuilder;
```

The only thing to note here is that the `args` field is the actual packed buffer of null-terminated args

### `zmm_cmd_initbuf`

```c
void zmm_cmd_initbuf(ArgvBuilder* cmd, void* argbuf, usize argbuf_size, void* argptr_buf, usize argptr_buf_size);
```

Initializes the ArgvBuilder struct with provided buffers. Useful for bypassing heap allocations with stack buffers. NOTE: must be freed with zmm_cmd_free().

### `zmm_cmd_append`

```c
void zmm_cmd_append(ArgvBuilder* cmd, SliceCU8 arg);
```

Appends an argument to the command.

### `zmm_cmd_appendz`

```c
void zmm_cmd_appendz(ArgvBuilder* cmd, const SliceCU8* args);
```

Appends a list of arguments to the command.

### `zmm_cmd_pappend`

```c
void zmm_cmd_pappend(ArgvBuilder* cmd, SliceCU8 arg);
```

Appends part of an argument (partial append).

### `zmm_cmd_pfinish`

```c
void zmm_cmd_pfinish(ArgvBuilder* cmd);
```

Finishes the current partial argument.

### `zmm_cmd_free`

```c
void zmm_cmd_free(ArgvBuilder* cmd);
```

Frees the command builder's buffers.

### `zmm_arg_init`

```c
i32 zmm_arg_init(Argv* self, int argc, char** argv);
```

Initializes the Argv struct from standard C argc/argv. Returns 0 on success, -1 on failure. NOTE: must be freed with zmm_arg_free().

### `zmm_arg_free`

```c
void zmm_arg_free(Argv* self);
```

Frees the Argv struct.

### `zmm_arg_contains`

```c
bool zmm_arg_contains(const Argv* self, SliceCU8 flag);
```

Returns whether a flag is present in the arguments.

### `zmm_arg_get_value`

```c
SliceCU8 zmm_arg_get_value(const Argv* self, SliceCU8 key);
```

Returns the value of a conjoined argument like `--port=8080`, or a nullslice if no match is found.

### `zmm_arg_get_subsequent`

```c
SliceCU8 zmm_arg_get_subsequent(const Argv* self, SliceCU8 key);
```

Returns the value of a separated argument like `--port 8080`, or a nullslice if it does not exist.

## arr.h

### `arr(T)`

This header just contains some macros to explicitly signify what is a dynamic array, a hashmap.

```c
#define arr(T) T*
```

Declares a dynamic array of type T. Uses stb_ds.h under the hood.

### `arrinit`

```c
#define arrinit NULL
```

Initializer for dynamic arrays.

### `stringhm(T)`

```c
#define stringhm(T) T*
```

Declares a string hash map with values of type T. Uses stb_ds.h under the hood.

### `stringhm_init`

```c
#define stringhm_init NULL
```

Initializer for string hash maps.

## builder.h

### BuilderFn

```c
typedef int (*BuilderFn)(const SliceCU8* sources, usize num_sources,
                         SliceCU8 output, u32 thread_idx);
```

This is the builder function that the builder invoke to actually generate the output files from the sources.
If it returns a non-zero value, the build is aborted.

Typically this will call a compiler, linker, assembler via `zmm_sys_exec_print` or something alike

### `zmm_builder_init`

```c
void zmm_builder_init(BuildGraph* g);
```

Initialize the build graph.

### `zmm_builder_free`

```c
void zmm_builder_free(BuildGraph* g);
```

Free all memory associated with the graph.

### `zmm_builder_add`

```c
void zmm_builder_add(BuildGraph* g, SliceCU8 const* sources, usize num_sources, SliceCU8 output, SliceCU8 const* deps, usize num_deps);
```

Add a target to the DAG.

* `sources`: Array of source files.
* `num_sources`: Number of source files.
* `output`: The output target file.
* `deps`: Optional array of extra dependencies.
* `num_deps`: Number of extra dependencies.

NOTE: if a dependency has length 0, the target will always be dirty.

### `zmm_builder_build`

```c
int zmm_builder_build(BuildGraph* g, SliceCU8 target, BuilderFn builder);
```

Execute the build for a specific target, isolating its subgraph. Runs tasks in parallel using a thread pool. Returns 0 on success, or non-zero if a build task failed.

## cc.h

### `zmm_cc_parse`

```c
i32 zmm_cc_parse(CompileCommands* cc, ArenaAlloc* arena, SliceCU8 path);
```

Parses a compile_commands.json file into the CompileCommands struct. Strings are backed by the provided arena. Returns 0 on success, -1 on failure.

### `zmm_cc_append`

```c
i32 zmm_cc_append(CompileCommands* cc, ArenaAlloc* arena, SliceCU8 file, const char* args, usize num_args);
```

Appends a compile command to the list.

* `args`: Packed array of null-terminated strings. (each arg in one buffer, separated by NULL)
* `num_args`: Number of arguments in the packed array.

the current_directory in the CompileCommands struct is used implicitly.

### `zmm_cc_append_dir`

```c
i32 zmm_cc_append_dir(CompileCommands* cc, ArenaAlloc* arena, SliceCU8 file,
                          const char* args, usize num_args, SliceCU8 dir);
```

Appends a compile command with a specific directory. Updates the current_directory in cc

### `zmm_cc_write`

```c
i32 zmm_cc_write(CompileCommands* cc, SliceCU8 path);
```

Writes the CompileCommands to a file in JSON format.

### `zmm_cc_free`

```c
void zmm_cc_free(CompileCommands* cc);
```

Frees the internal array of compile commands. NOTE: Does not free the arena-backed strings.

## cpu.h

### `zmm_cpu_init`

```c
void zmm_cpu_init(CpuInfo* info);
```

Populates the CpuInfo struct with x86 features and cache information.

### `zmm_cpu_supports(info_ptr, feat)`

```c
#define zmm_cpu_supports(info_ptr, feat) ((info_ptr)->x86.features.feat)
```

Returns whether a specific CPU feature is supported.

### `zmm_cpu_cache_size`

```c
i32 zmm_cpu_cache_size(const CpuInfo* info, int level, CacheType cache_type);
```

Returns the cache size in bytes for a given level and type. Returns 0 if missing.

### `zmm_cpu_l1d_size`

```c
i32 zmm_cpu_l1d_size(const CpuInfo* info);
```

Returns L1 Data Cache size in bytes.

### `zmm_cpu_l1i_size`

```c
i32 zmm_cpu_l1i_size(const CpuInfo* info);
```

Returns L1 Instruction Cache size in bytes.

### `zmm_cpu_l2_size`

```c
i32 zmm_cpu_l2_size(const CpuInfo* info);
```

Returns L2 Unified (or Data) Cache size in bytes.

### `zmm_cpu_l3_size`

```c
i32 zmm_cpu_l3_size(const CpuInfo* info);
```

Returns L3 Unified Cache size in bytes.

### `zmm_cpu_thread_count`

```c
i32 zmm_cpu_thread_count();
```

Returns the number of online logical processors.

### `zmm_cpu_brand_string`

```c
SliceU8 zmm_cpu_brand_string(const CpuInfo* info);
```

Returns the full CPU brand string. NOTE: The resulting slice's ptr must be freed.

### `zmm_cpu_vendor`

```c
SliceU8 zmm_cpu_vendor(const CpuInfo* info);
```

Returns the CPU Vendor ID string. NOTE: The resulting slice's ptr must be freed.

### `zmm_cpu_uarch_str`

```c
SliceU8 zmm_cpu_uarch_str(const CpuInfo* info);
```

Returns a string representation of the Microarchitecture. NOTE: The resulting slice's ptr must be freed.

### `zmm_cpu_family`

```c
i32 zmm_cpu_family(const CpuInfo* info);
```

Returns the CPU family ID.

### `zmm_cpu_model`

```c
i32 zmm_cpu_model(const CpuInfo* info);
```

Returns the CPU model ID.

### `zmm_cpu_stepping`

```c
i32 zmm_cpu_stepping(const CpuInfo* info);
```

Returns the CPU stepping ID.

## dep.h

### `zmm_dep_parse`

```c
arr(SliceU8) zmm_dep_parse(SliceCU8 path);
```

Parses a compiler dependency file (.d) and returns an array of dependencies as slices. Returns stb_ds array of SliceU8. NOTE: The returned array must be freed via `slicearr_free`

## fs.h

### FindDepth

```c

typedef enum : u8 { FD_SHALLOW, FD_RECURSIVE } FindDepth;
```

### FindKind

```c

typedef enum : u8 { FK_FILE = 1, FK_DIRECTORY } FindKind;
```

### FindOpts

```c
typedef struct {
    FindDepth depth;

    const FindKind* kinds;  // null-terminated

    const SliceCU8* exts;  // nullslice-terminated

    bool ignore_hidden;
} FindOpts;
```

This struct defines options for filesystem sarching.

### `zmm_fs_find`

```c
i32 zmm_fs_find(arr(SliceU8) * out, const SliceCU8* roots, FindOpts opts);
```

Finds all matches given the roots and find options, appends them to out. Returns 0 on success, otherwise an error code. NOTE: The result must be freed with `slicearr_free()`.

### `zmm_fs_create_dir_path`

```c
i32 zmm_fs_create_dir_path(SliceCU8 path);
```

Creates a directory and all its parents (equivalent to `mkdir -p`).

### `zmm_fs_create_parent_path`

```c
i32 zmm_fs_create_parent_path(SliceCU8 path);
```

Creates the parent directory of a file path (this can create multiple parent dirs).

### `zmm_fs_file_exists`

```c
bool zmm_fs_file_exists(SliceCU8 path);
```

Returns whether a file exists.

### `zmm_fs_delete_file`

```c
i32 zmm_fs_delete_file(SliceCU8 path);
```

Deletes a file.

### `zmm_fs_delete_dir`

```c
i32 zmm_fs_delete_dir(SliceCU8 path);
```

Deletes an empty directory.

### `zmm_fs_delete_tree`

```c
i32 zmm_fs_delete_tree(SliceCU8 path);
```

Recursively deletes a directory tree.

### `zmm_fs_abs_cwd`

```c
SliceU8 zmm_fs_abs_cwd(void);
```

Returns the absolute path of the current working directory. NOTE: The resulting slice's ptr must be freed.

### `zmm_fs_change_cwd`

```c
i32 zmm_fs_change_cwd(SliceCU8 path);
```

Changes the current working directory.

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

Returns the extension of `path` as a slice, or a nullslice if there is none.

### `zmm_p_stem`

```c
SliceCU8 zmm_p_stem(SliceCU8 path);
```

Returns the stem (filename without extension) of `path`.

### `zmm_p_strip_ext`

```c
SliceCU8 zmm_p_strip_ext(SliceCU8 path);
```

Returns the entire path with the extension removed.

### `zmm_p_has_ext`

```c
bool zmm_p_has_ext(SliceCU8 path, SliceCU8 ext);
```

Returns whether the path has the specified extension.

### `zmm_p_has_exts`

```c
bool zmm_p_has_exts(SliceCU8 path, const SliceCU8* exts);
```

Returns whether the path has any of the specified extensions.
`exts` must be a Nullslice-terminated array.

### `zmm_p_is_hidden`

```c
bool zmm_p_is_hidden(SliceCU8 path);
```

Returns whether the file is hidden (starts with a dot).

### `zmm_p_join_any`

```c
SliceU8 zmm_p_join_any(const SliceCU8* parts, u8* stack_buf, usize stack_buf_size, u8** heap_out);
```

Joins multiple path parts together. it handles directories and strips redundant "./" from paths.

joining logic:

* if a part ends in a `/`, it is assumed to be a directory, and a single slash is kept
* otherwise, the part is assumed to be a filename or part of a filename. no separator is used.

allocation:

* if the stack_buf is large enough to fit the result, it will be put there.
* otherwise, memory is allocated and the pointer is put into *heap_out
* when no heap allocation occurs, *heap_out is set to nullptr.

NOTE: the resulting *heap_out must be freed if it is not a nullptr.

### `zmm_p_trim_dot_slash`

```c
SliceCU8 zmm_p_trim_dot_slash(SliceCU8 path);
```

Trims leading "./" from a path.

### `zmm_p_is_separator(char c)`

```c
static inline bool zmm_p_is_separator(char c);
```

Returns whether a character is a path separator.

### `zmm_p_normalize_sep(SliceU8 path)`

```c
inline static void zmm_p_normalize_sep(SliceU8 path);
```

Normalizes all separators in a path to forward slashes.

## print.h

Custom `printf` implementation.
Supported format specifiers:

* `%i8`, `%i16`, `%i32`, `%i64`, `%u8`, `%u16`, `%u32`, `%u64`
* `%usz` (usize), `%ssz` (ssize)
* `%d` (i64), `%f` (double), `%x` (u64 hex), `%p` (pointer)
* `%c` (char), `%s` (SliceCU8), `%sz` (null-terminated char*)
* `%b` (bool)

### `zmm_snprintf`

```c
usize zmm_snprintf(char* str, usize size, const char* format, ...);
```

Formats a string into a buffer with a size limit.

### `zmm_sprintf`

```c
usize zmm_sprintf(char* str, const char* format, ...);
```

Formats a string into a buffer.

### `zmm_printf`

```c
usize zmm_printf(const char* format, ...);
```

Prints a formatted string to stdout.

### `zmm_lprintf`

```c
usize zmm_lprintf(const char* format, ...);
```

Prints to a thread-local buffer. Use zmm_lemit() to output the buffer thread-safely.

### `zmm_lemit`

```c
void zmm_lemit();
```

Emits the accumulated thread-local buffer to stdout thread-safely.

## slice.h

### `Slice(T)`

```c
#define Slice(T) ...
```

Declares a slice of type T.

### `strlit(S)`

```c
#define strlit(S) ...
```

Creates a SliceCU8 from a string literal.

### `slice_to_cstr(s)`

```c
#define slice_to_cstr(s) ...
```

Converts a Slice to a null-terminated C-string. result must be freed with free().

### `slicelit`, `slicearr`

Macros for creating slice literals and arrays of slices.

### `slice_eq(a, b)`

```c
#define slice_eq(a, b) ...
```

Returns whether two slices are equal.

### `slicearr_free(arr)`

```c
#define slicearr_free(arr) ...
```

Frees an stb_ds array of slices and each slice's ptr.

## sys.h

### TermType

```c
typedef enum {
    TERM_EXITED, // returned an exit code
    TERM_SIGNALED, // forcibly terminated by OS
    TERM_ERROR  // failed to even start the process
} TermType;
```

### ChildTerm

```c
typedef struct {
    TermType type;
    i32 code;  // only valid if TermType is TERM_EXITED
} ChildTerm;
```

### ExecStatus

```c
typedef struct {
    ChildTerm term;
    ExecError err;
} ExecStatus;
```

Struct representing the state of the execution

### ExecResult

```c
typedef struct {
    ExecStatus status;
    SliceU8 output;
} ExecResult;
```

represents the execution status with the allocated output buffer

### `exec_result_free`

```c
void exec_result_free(ExecResult* res);
```

Frees the output buffer allocated by zmm_sys_exec.

### `zmm_sys_exec`

```c
ExecResult zmm_sys_exec(const char* arg_buf, usize num_args);
```

Executes a command, merging stderr into stdout at the OS level. Returns ExecResult containing the output buffer and execution status. NOTE: The caller must call exec_result_free() on the returned struct.

### `zmm_sys_exec_print`

```c
ExecStatus zmm_sys_exec_print(const char* arg_buf, usize num_args);
```

Executes a command, merges output, prints it thread-safely, and frees the output buffer. Returns the execution status (error and return code).

### `zmm_sys_exec_redirect`

```c
ExecStatus zmm_sys_exec_redirect(const char* arg_buf, usize num_args);
```

Executes a command without capturing output (inherits stdio from parent).
