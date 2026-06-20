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


#include <stb/stb_ds.h>
#include <stdio.h>

#include "args.h"
#include "arr.h"
#include "builder.h"
#include "cc.h"
#include "cpu.h"
#include "dep.h"
#include "fs.h"
#include "path.h"
#include "print.h"
#include "slice.h"
#include "sys.h"

int builder(const SliceCU8* sources, usize num_sources, SliceCU8 output,
            u32 thread_idx) {
    // you would call clang, gcc, nasm, or something else here to actually
    // genrate the output command via a zmm_sys_exec
    bool is_exe = zmm_p_has_ext(output, strlit(".out"));

    if (is_exe) {
        zmm_lprintf("thread %u32 is linking [", thread_idx);

        for (usize i = 0; i < num_sources; i++) {
            zmm_lprintf(" %s", sources[i]);
        }

        zmm_lprintf(" ] -> %s\n", output);
    }

    else {
        zmm_lprintf("thread %u32 is compiling [", thread_idx);

        for (usize i = 0; i < num_sources; i++) {
            zmm_lprintf(" %s", sources[i]);
        }

        zmm_lprintf(" ] -> %s\n", output);
    }

    zmm_lemit();

    return 0;
}

int main(int argc, char** argv) {
    // argv parsing
    Argv args;
    zmm_args_init(&args, argc, argv);

    zmm_printf("did you pass the -ffast flag?: %b\n",
               zmm_args_contains(&args, strlit("-ffast")));

    zmm_args_free(&args);

    // file discovery
    arr(SliceU8) a = NULL;

    zmm_fs_find(&a, (SliceCU8[]){strlit("./"), NullSliceCU8},
                (FindOpts){
                    .depth = FD_RECURSIVE,
                    .exts = (SliceCU8[]){strlit(".c"), NullSliceCU8},
                    .kinds = (FindKind[]){FK_FILE, 0},
                    .ignore_hidden = true,
                });

    usize len = arrlen(a);

    for (usize i = 0; i < len; i++) {
        SliceU8 f = a[i];

        zmm_printf("%s\n", f);
    }

    // path manipulation: turning source file path (e.g. src/test/main.c ->
    // build/.obj/src/test/main.o)

    u8 path_buf[1024];

    SliceCU8 obj_dir = strlit("build/.obj/");
    SliceCU8 obj_ext = strlit(".obj");

    for (usize i = 0; i < len; i++) {
        SliceU8 src = a[i];

        SliceCU8 noext = zmm_p_strip_ext(slice_cast(SliceCU8, src));

        u8* heap;
        SliceU8 obj = zmm_p_join_any(                                   //
            slicearr(SliceCU8, obj_dir, noext, obj_ext, NullSliceCU8),  //
            path_buf, sizeof(path_buf), &heap);

        zmm_printf("%s\n", obj);

        if (heap) free(heap);
    }

    slicearr_free(a);

    // command building & putting it in compile commands

    ArgvBuilder cmd;

    char cmd_buf[1024];
    char cmd_ptr_buf[256];

    zmm_argv_initbuf(&cmd, cmd_buf, sizeof(cmd_buf), cmd_ptr_buf,
                     sizeof(cmd_ptr_buf));

    zmm_argv_appendz(
        &cmd, slicearr(SliceCU8, strlit("clang"), strlit("-O3"), NullSliceCU8));

    zmm_argv_append(&cmd, strlit("test.c"));

    zmm_argv_pappend(&cmd, strlit("-"));
    zmm_argv_pappend(&cmd, strlit("g"));
    zmm_argv_pfinish(&cmd);

    const char* p = cmd.flat;
    for (usize i = 0; i < cmd.num_args; i++) {
        usize len = strlen(p);

        zmm_printf("%sz\n", p);

        p += len + 1;
    }

    ArenaAlloc arena;
    zmm_arena_init(&arena);

    CompileCommands cc;
    zmm_cc_init(&cc, strlit("new_cc.json"));

    zmm_cc_append(&cc, strlit("./foo"), strlit("hello.c"), cmd.flat,
                  cmd.num_args);

    zmm_cc_finish(&cc);

    zmm_argv_free(&cmd);

    // cpu info

    CpuInfo cpu;

    zmm_cpu_init(&cpu);

    bool ifma = zmm_cpu_supports(&cpu, avx512ifma);
    bool bmi2 = zmm_cpu_supports(&cpu, bmi2);

    SliceU8 cpu_name = zmm_cpu_brand_string(&cpu);

    zmm_printf("your cpu: %s\n", cpu_name);

    free(cpu_name.ptr);

    zmm_printf("ifma: %b, bmi2: %b\n", ifma, bmi2);

    // command execution

    zmm_sys_exec_redirect_flat("ls\0-al\0", 2);

    zmm_sys_exec_print_flat("ls\0-al\0", 2);

    // DAG creation & execution

    BuildGraph bg;

    zmm_bg_init(&bg, &arena);

    zmm_bg_add(&bg,  //
               slicearr(SliceCU8, strlit("test1.c")), 1,
               strlit("obj/test1.o"),  //
               NULL, 0);

    zmm_bg_add(&bg,  //
               slicearr(SliceCU8, strlit("test2.c")), 1,
               strlit("obj/test2.o"),  //
               NULL, 0);

    zmm_bg_add(&bg,  //
               slicearr(SliceCU8, strlit("test3.c")), 1,
               strlit("obj/test3.o"),  //
               NULL, 0);

    zmm_bg_add(&bg,  //
               slicearr(SliceCU8, strlit("test4.c")), 1,
               strlit("obj/test4.o"),  //
               NULL, 0);

    zmm_bg_add(&bg,  //
               slicearr(SliceCU8, strlit("test5.c")), 1,
               strlit("obj/test5.o"),  //
               NULL, 0);

    zmm_bg_add(&bg,  //
               slicearr(SliceCU8,
                        strlit("obj/test1.o"),  //
                        strlit("obj/test2.o"),  //
                        strlit("obj/test3.o"),  //
                        strlit("obj/test4.o"),  //
                        strlit("obj/test5.o"),  //
                        ),
               5,
               strlit("build/test2.out"),  //
               NULL, 0);

    zmm_bg_build(&bg, slicearr(SliceCU8, strlit("build/test2.out")), 1,
                 builder);

    zmm_bg_free(&bg);

    // dependency file parsing:

    SliceCU8 dep_file = strlit("./build/.deps/fs.d");

    arr(SliceU8) deps = arrinit;

    zmm_dep_parse(&deps, dep_file);

    for (usize i = 0; i < arrlenu(deps); i++) {
        SliceU8 dep = deps[i];

        zmm_printf("dependency: %s\n", dep);
    }

    slicearr_free(deps);

    zmm_arena_free(&arena);
}
