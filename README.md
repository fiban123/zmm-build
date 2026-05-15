# zmm-build

**zmm** is an ultra low-level, zero-magic compiled build system designed for maximum control and speed.

Written entirely in C, `zmm` doesn't generate make or ninja files, it builds your project *directly*. Your build script is a compiled C program, which means it executes at blistering speeds (compilation of the build script itself usually takes 4-48ms).

While `zmm` can be used for any regular project, it was purpose-built for **assembly projects** that require fine-grained control over compile-time dynamic assembly generation, macro passing, and architecture-specific implementations (e.g. multiprecision libraries with targeted asm and fallback C implementations).

---

## Table of Contents

- [Why zmm?](#why-zmm)
  - [Bloat Comparison](#bloat-comparison)
  - [Use-Case difficulty comparison](#use-case-difficulty-comparison)
- [Features](#features)
- [Getting Started](#getting-started)
  - [Requirements](#requirements)
  - [Installation](#installation)
  - [CLI](#cli)
  - [Quick look](#quick-look)
  - [Available templates](#available-templates)
  - [Docs](#docs)
- [License](#license)

---

## Why zmm?

- **Zero Abstractions:** Avoid the pitfalls of "black box" build systems. If you want to link a file, you manually invoke the linker. You'll never have to ask *"Which keyword does X?"* because you are writing standard C.
- **Blazing Fast:** Compiling a C-based build script takes 30-45ms (~4ms with tcc).
- **Platform:** Runs flawlessly across platforms, including Windows with POSIX-emulation software.
- **Hardware Aware:** Built-in CPU capability detection (e.g., AVX512F, BMI2) allows you to effortlessly route builds based on the host architecture.
- **Tiny Footprint:** The `zmm` shared library is incredibly lightweight (81KB) compared to industry standards.

### Bloat Comparison

| Build System | File size Bloat |
| :--- | :--- |
| **CMake** | ~135x more bloated than zmm (11MB) |
| **GNU Make** | ~3.48x more bloated than zmm (282KB) |
| **zmm** | **81KB** (peak) |

>note: The CMake 11MB is coming from purely the CMake executable. This doesn't include cpack, ctest, ccmake, cmake-gui and the default cmake modules (which all come with cmake)

### Use-Case difficulty comparison

| Build System | Small C project | Big C project | Comptime CPU-based dispatch Asm Multiprecision library |
| :--- | :--- | :--- | :--- |
| CMake | Easy | Moderate | Impossible without external tools |
| GNU Make | Moderate | Moderate | Nightmare |
| zmm (no templates) | Hard | Hard | Easy |
| zmm (with templates) | Easy | Moderate | Easy |

---

## Features

Despite being extremely low-level, the `zmm` library provides a powerful suite of tools to make writing your build script as straightforward as C allows:

- **Multithreaded DAG Execution:** Build graphs that automatically handle parallel compilation and linking.
- **Thread-Safe by Default:** Safely handle concurrent compiler invocations and utilize the custom and thread-safe printing (`zmm_printf`).
- **Modern String Handling:** Employs slices instead of fragile null-terminated strings for safer and more efficient text manipulation.
- **Filesystem & Path Primitives:** Comprehensive tools for path manipulation, recursive file discovery (with hidden/extension filtering), changing working directories, and deleting files.
- **Command & CLI Tools:** Safe string building for compiler/linker arguments, backed by an `argv` parser to easily handle custom build flags.
- **Compilation & Dependency Parsing:** Native support for parsing compiler dependency files, as well as parsing, modifying, and writing `compile_commands.json` for LSP support.
- **CPU Info:** Detect CPU brand strings and specific instruction set support easily.

---

## Getting Started

### Requirements

#### Compiling & Installing libzmm

- C23 (gnu23) compiler (gcc / clang, msvc not supported)
- `libcpu_features`
- `libc`
- GNU `make`
- `MSYS2` on windows

>Since CPU features are constantly changing, and so is the cpu_features library, I cannot
just include some version in the source code of this repo (unlike jsmn and stb_ds). Otherwise,
the CPU features would just become outdated. On windows this means you have to follow the build
instructions from the cpu_features github (you'll need CMake) since MSYS2
does not have a package of it. On linux, some distros have a package of it.

#### Compiling a build script

- C99 or higher compiler (gcc / clang / tcc, msvc not supported)
- `libcpu_features`
- `libzmm` (obviously)
- `libc`

### Installation

#### POSIX systems

##### Installing `libcpu_features`

Some distros have a package for it, but most don't.

If you are on Arch, simply `yay -S libcpufeatures-git`.

Otherwise, follow build & install instructions [here](https://github.com/google/cpu_features#cmake)

##### Installing `zmm`

This works for all distros.

```bash
git clone https://github.com/fiban123/zmm.git
cd zmm

make fast
sudo make install
```

#### Windows

##### Installing `libcpu_features`

Follow build & install instructions [here](https://github.com/google/cpu_features#cmake)

##### Installing `zmm`

In the `MSYS2` shell:

```bash
git clone https://github.com/fiban123/zmm.git
cd zmm

make fast
make install PREFIX=ucrt64/
```

> `ucrt64` runtime is recommended, but anything other than `msvc` runtime should work.

### CLI

Of course, you don't need to manually compile your build scripts when you change them.
A super lightweight CLI will check if the build executable is up-to-date, and rebuild it if it isn't.
It uses a small configuration file for the compilation command and other basic info.

See more in the [docs](DOCS.md#zmm-CLI)

---

### Quick look

Because `zmm` build scripts are written in C, you *need* to be comfortable with pointers and manual memory management. However, **drop-in templates** are available to get your project set up quickly without starting from scratch. (see [Available Templates](#available-templates))

Here is a simplified example of what a `zmm` build script and config can look like. It detects CPU features, finds source files, and builds a multithreaded dependency graph (DAG). I will not hide the fact that even basic build.c files using
zmm are already pretty long and perhaps complex. If you want to look at a real build.c file using zmm, take a look
at one of the templates.

#### `build.c`

```c
#include <zmm/zmm.h>

// The actual execution step for the DAG
int builder(const SliceCU8* sources, usize num_sources, SliceCU8 output, u32 thread_idx) {
    bool is_exe = zmm_p_has_ext(output, strlit(".out"));

    // You would call clang, gcc, or an assembler here via zmm_sys_exec
    if (is_exe) {
        zmm_lprintf("thread %u32 is linking -> %s\n", thread_idx, output);
    } else {
        zmm_lprintf("thread %u32 is compiling -> %s\n", thread_idx, output);
    }
    zmm_lemit();
    return 0;
}

int main() {
    // 1. Check CPU capabilities for architecture-specific builds
    CpuInfo cpu;
    zmm_cpu_init(&cpu);
    bool has_avx512 = zmm_cpu_supports(&cpu, avx512ifma);

    SliceU8 cpu_name = zmm_cpu_brand_string(&cpu);
    zmm_printf("Host CPU: %s (AVX-512: %b)\n", cpu_name, has_avx512);
    free(cpu_name.ptr);

    ArenaAlloc arena;
    zmm_arena_init(&arena);

    // 2. Define the Build Graph (DAG)
    BuildGraph bg;
    zmm_bg_init(&bg, &arena);

    // Add object targets
    zmm_bg_add(&bg, slicearr(SliceCU8, strlit("test1.c")), 1,
                    strlit("obj/test1.o"), NULL, 0);
    zmm_bg_add(&bg, slicearr(SliceCU8, strlit("test2.c")), 1,
                    strlit("obj/test2.o"), NULL, 0);

    // Add executable target depending on objects
    zmm_bg_add(
        &bg, slicearr(SliceCU8, strlit("obj/test1.o"), strlit("obj/test2.o")),
        2, strlit("build/test.out"), NULL, 0);

    // 3. Execute the build
    zmm_bg_build(&bg, strlit("build/test.out"), builder);
    zmm_bg_free(&bg);
    zmm_arena_free(&arena);

    return 0;
}
```

#### `.zmm` config file

```json
{
    "build-script-srcs": [
        "./build.c"
    ],
    "build-script-exe": "./build.out",
    "build-script-compile-cmd": "clang -Og ./build.c -o ./build.out -lzmm"
}
```

#### Executing the build script

To run this build script with the zmm CLI:

```bash
zmm
```

> yes, the build command is usually literally just that.

If you wanted to run the build script with a `-ffast` arg (does not exist in this example):

```bash
zmm -ffast
```

---

### Available templates

Templates can be found in [templates/](templates/)

> The templates have not been extensively tested, they may be buggy.

#### `minimal-exe` (238 LOC)

A configurable, lightweight and straightforward template suitable for basic projects.

- **Behavior:** Recursively discovers all `.c` files in the `src/` directory, then compiles and links them into a single executable (`build/exe.out`).
- **Outputs:** Places object files in `build/.obj/` and dependency files in `build/.dep/`. Automatically generates a `compile_commands.json` at the project root for LSP support.
- **Flags:** Debug information (`-g`) is enabled by default.
  - `-ffast`: Appends the `-O3` optimization flag.
  - `-fsan`: Enables `-fsanitize=address` and `-fsanitize=undefined` for debugging.

- **Caveat:** Does not track flag modifications between incremental runs. For example, if you add `-ffast` without cleaning first, only newly modified files will receive the optimization.
- **Arguments:**
  - `clean`: Deletes all build artifacts.

- **Build Script Compile Times:** GCC: ~32ms | Clang: ~40ms | TCC: ~4ms

#### `practical-exe` (318 LOC)

A practical and configurable template for single executable projects.

- **Behavior:** Compiles all `.c` files in `src/` into object files and links them into `build/exe.out`.
- **Outputs:** Places object files in `build/.obj/` and dependency files in `build/.dep/`. Generates `compile_commands.json` at the project root.
- **Flags:** Debug information (`-g`) is enabled by default.
  - `-ffast`: Appends the `-O3` optimization flag.
  - `-fsan`: Enables Address and Undefined behavior sanitizers.
  - `-fno-debug`: Strips the `-g` flag for clean releases.

- **Isolated flag directories:** Handles flag changes perfectly. It mangles CLI arguments into unique build subdirectories, ensuring different build configurations live in isolation. With this, you basically never need to clean the project.
- **Arguments:**
  - `clean`: Deletes all build artifacts.

- **Build Script Compile Times:** GCC: ~40ms | Clang: ~45ms | TCC: N/A

#### `practical-lib` (450 LOC)

A practical and configurable template made for building libraries.

- **Behavior:** Compiles all `.c` files in `src/` to build static libraries, dynamic libraries, or both. It also supports defining dedicated test / main files that link against the generated library.
- **Outputs:** Places object files in `build/.obj/` and dependency files in `build/.dep/`. Generates `compile_commands.json` at the project root.
- **Flags:** Debug information (`-g`) is enabled by default.
  - `-ffast`: Appends the `-O3` flag.
  - `-fsan`: Enables Address and Undefined behavior sanitizers.
  - `-fno-debug`: Strips the `-g` flag.

- **Isolated flag directories:** Handles flag changes perfectly. It mangles CLI arguments into unique build subdirectories, ensuring different build configurations live in isolation. With this, you basically never need to clean the project.
- **Arguments:**
  - `clean`: Deletes all build artifacts.
  - `lib`: Component-specific build that targets only the library (defaults to a shared library).
  - *Default:* Builds the core library alongside all specified test executables.

- **Build Script Compile Times:** GCC: ~44ms | Clang: ~48ms | TCC: N/A

---

### Docs

The full documentation for libzmm API, and zmm CLI are in [DOCS.md](DOCS.md)

## License

The core **zmm** build system library and CLI are licensed under the [GNU GPLv3 license](LICENSE).

However, all files inside the `templates/` directory are dedicated to the public domain under [The Unlicense](templates/LICENSE). You are free to copy, modify, publish, use, compile, sell, or distribute them in any form, commercial or non-commercial, without any attribution or restrictions.

For licensing inquiries, please open an issue or a discussion directly in this GitHub repository.
