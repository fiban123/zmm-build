# zmm-build

**zmm** is an ultra low-level, zero-abstraction compiled build system designed for maximum control and speed.

Written entirely in C, `zmm` doesn't just configure your build—it *is* your build. Your build script is a compiled C program, which means it executes at blistering speeds (compilation of the build script itself takes roughly 5ms).

While `zmm` can be used for any regular project, it was purpose-built for **assembly projects** that require fine-grained control over dynamic assembly generation, macro passing, and architecture-specific implementations (e.g. multiprecision libraries with targeted asm or fallback C implementations).

---

## Why zmm?

* **Zero Abstractions:** Avoid the pitfalls of "black box" build systems. If you want to link a file, you manually invoke the linker. You'll never have to ask *"how do I do X in this build system"* because you are writing standard C.
* **Blazing Fast:** Compiling a C-based build script takes 30-40ms (4-5ms with tcc).
* **Native Cross-Platform:** Runs flawlessly across platforms, including Windows, without requiring any POSIX-emulation software.
* **Hardware Aware:** Built-in CPU capability detection (e.g., AVX512F, BMI2) allows you to effortlessly route builds based on the host architecture.
* **Tiny Footprint:** The `zmm` shared library is incredibly lightweight (68KB) compared to industry standards.

### Bloat Comparison

| Build System | File size Bloat |
| :--- | :--- |
| **CMake** | ~161x more bloated than zmm (11MB) |
| **GNU Make** | ~4.14x more bloated than zmm (282KB) |
| **zmm** | **68KB** (peak) |

>note: The CMake 11MB is coming from purely the CMake executable. this doesnt include cpack, ctest, ccmake, cmake-gui and the default cmake modules (which all come with cmake)

### Use-Case difficulty comparison

| Build System | Small C project | Big C project | Comptime CPU-based dispatch Asm Multiprecision library |
| :--- | :--- | :--- | :--- |
| CMake | Easy | Moderate | Impossible without external tools |
| GNU Make | Moderate | Moderate | Nightmare |
| zmm (no templates) | Hard | Hard | Easy |
| zmm (with templates) | Easy | Moderate | Easy |

## Features

Despite being extremely low-level, the `zmm` library provides a powerful suite of tools to make writing your build script as straightforward as C allows:

* **Multithreaded DAG Execution:** Build graphs that automatically handle parallel compilation and linking.
* **Thread-Safe by Default:** Safely handle concurrent compiler invocations and utilize our custom, ergonomic, and thread-safe printing (`zmm_printf`).
* **Modern String Handling:** Employs slices instead of fragile null-terminated strings for safer and more efficient text manipulation.
* **Filesystem & Path Primitives:** Comprehensive tools for path manipulation, recursive file discovery (with hidden/extension filtering), changing working directories, and deleting files.
* **Command & CLI Tools:** Safe string building for compiler/linker arguments, backed by a built-in `argv` parser to easily handle custom build flags.
* **Compilation & Dependency Parsing:** Native support for parsing compiler dependency files, as well as parsing, appending, and writing `compile_commands.json` for tooling support.
* **CPU Info:** Detect CPU brand strings and specific instruction set support natively.

---

## Getting Started

Because `zmm` build scripts are written in C, you need to be comfortable with pointers and manual memory management. However, **drop-in templates** are available to get your project set up quickly without starting from scratch.

### Quick look

Here is a simplified example of what a `zmm` build script can look like. It detects CPU features, finds source files, and builds a multithreaded dependency graph (DAG).

```c
#include "builder.h"
#include "cpu.h"
#include "fs.h"
#include "print.h"
#include "sys.h"

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

    // 2. Define the Build Graph (DAG)
    BuildGraph bg;
    zmm_builder_init(&bg);

    // Add object targets
    zmm_builder_add(&bg, slicearr(SliceCU8, strlit("test1.c")), 1,
                    strlit("obj/test1.o"), NULL, 0);
    zmm_builder_add(&bg, slicearr(SliceCU8, strlit("test2.c")), 1,
                    strlit("obj/test2.o"), NULL, 0);

    // Add executable target depending on objects
    zmm_builder_add(
        &bg, slicearr(SliceCU8, strlit("obj/test1.o"), strlit("obj/test2.o")),
        2, strlit("build/test.out"), NULL, 0);

    // 3. Execute the build
    zmm_builder_build(&bg, strlit("build/test.out"), builder);
    zmm_builder_free(&bg);

    return 0;
}
```

### docs

the full documentation for libzmm, and zmm CLI are in DOCS.md

### templates

templates can be found in `templates/`

* simple.c:
  * this template finds all `.c` files in src/ directory and compiles & links them into an executable.
  * object files are put in `build/.obj/`
  * dep files are generated and put in `build/.deps/`
  * the resulting executable it put in `build/exe.out`
  * compile_commands are generated and put in the project root
  * cflags:
    * `-ffast`: adds `-O3`
  * debug info is always enabled (`-g`)
  * it does not handle differing flags cleanly. when you compile the project with no `-ffast`
 edit one file, and run it again with `-ffast`, only one object file will actually be optimized
  * args:
    * `c`: cleans the project and exits
    * `cb`: cleans the project and rebuilds it
  * build script compile-times: gcc: 32ms, clang: 40ms, tcc: 4ms

TODO: remake this

* generic.c:
  * comprehensive template that fixes the pitfalls of `simple.c`, but adds complexity
  * this template finds all `.c` files in the src/ directory and compiles & links them into an executable.
  * object files are put in `build/.obj/`
  * dep files are generated and put in `build/.deps/`
  * the resulting executable it put in `build/exe.out`
  * compile_commands are generated and put in the project root
  * cflags:
    * `-ffast`: adds `-O3`
    * `-flto`: adds `-flto`
    * `-fno-debug`: disables `-g` debug flag
    * `-fsan`: adds `-fsanitize=address` and `-fsanitize=undefined` for debugging
  * this template handles incremental builds with differing flags perfectly, differentiating fast
  and debug builds cleanly. so if you run a command with `-fsan`, it is guaranteed that every
  object file *will* have those flags. this increases compile time in theory, but the alternative
  would be that you have to manually run clean, and then rebuild it.
  * cache: this template needs to save the cflags used, so a tiny cache file (16 bytes) is used
  in build/.zmm_cache
  * args:
    * `-fno-cache`: this ignores the cache, and rebuilds only the needed objects.
    this no longer handles differing flags cleanly
    * `-fuse-default-dir`: this always puts all object files into the default (debug build)
    directory.
  * build script compile-time: ?

### Requirements

#### Compiling from source

* C23 compiler (gcc / clang, msvc not supported)
* `cpu_features` library
* `libc`

>Since CPU features are constantly changing, and so is the cpu_features library, I cannot
just include some version in the source code of this repo (unlike jsmn and stb_ds). Otherwise,
the CPU features would just become outdated. On windows this means you have to follow the build
instructions from the cpu_features github (you'll need CMake :( ) since MSYS2
does not have a package of it. But on linux, most distros have an easy package of it.

#### Compiling a build script

* C99 / C11 / C17 / C23 compiler (gcc / clang / tcc, msvc not supported)
* `cpu_features` library
* `libzmm` (obviously)
* `libc`
