/*
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 *
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * For more information, please refer to <https://unlicense.org/>
 */
// NOTE: you can remove this header if you like.

#include <zmm/builder.h>
#include <zmm/slice.h>
#include <zmm/zmm.h>

#ifdef _WIN32
#define SO_EXT ".dll"
#else
#define SO_EXT ".so"
#endif

// *** config ***

// compilation base flags
SliceCU8 CFLAGS_BASE[] = {strlit("-fPIC"), strlit("-std=gnu23"),
                          strlit("-fdiagnostics-color=always"),
                          strlit("-Iinclude/"), NullSliceCU8};

// linking base flags (nothing by default)
SliceCU8 LDFLAGS_BASE[] = {NullSliceCU8};
// exe linking base flags (link the lib and include it in rpath)
SliceCU8 LDFLAGS_EXE[] = {strlit("-Lbuild/"), strlit("-lmylib"),
                          strlit("-Wl,-rpath,'$ORIGIN/build'"), NullSliceCU8};
// lib linking base flags (link pthread as an example)
SliceCU8 LDFLAGS_LIB[] = {strlit("-lpthread"), NullSliceCU8};

// all base flags (compilation + linking)
SliceCU8 AFLAGS_BASE[] = {NullSliceCU8};

SliceCU8 ARFLAGS[] = {strlit("rcs"), NullSliceCU8};

// -ffast cflags
SliceCU8 CFLAGS_FAST[] = {strlit("-O3"), NullSliceCU8};
// no -ffast cflags (none by default)
SliceCU8 CFLAGS_NO_FAST[] = {NullSliceCU8};

// -fno-debug aflags (none by default)
SliceCU8 AFLAGS_NODEBUG[] = {NullSliceCU8};
// no -fno-debug aflags
SliceCU8 AFLAGS_NO_NODEBUG[] = {strlit("-g"), NullSliceCU8};

// -fsan flags
SliceCU8 AFLAGS_SAN[] = {strlit("-fsanitize=address"),
                         strlit("-fsanitize=undefined"), NullSliceCU8};
// no -fsan flags (none by default)
SliceCU8 AFLAGS_NO_SAN[] = {NullSliceCU8};

SliceCU8 CC = strlit("clang");
SliceCU8 AR = strlit("ar");

SliceCU8 TESTING_SRCS[] = {strlit("test/main.c"), NullSliceCU8};
SliceCU8 TESTING_EXES[] = {strlit("build/test.out"), NullSliceCU8};

#define M_SHLIB strlit("build/libmylib" SO_EXT)
#define M_STLIB strlit("build/libmylib.a")

// by default, only build the shared lib
SliceCU8 LIBS[] = {M_SHLIB, NullSliceCU8};

SliceCU8 CCMDS_FILE = strlit("compile_commands.json");

SliceCU8 OBJ_DIR = strlit("build/.obj/");
SliceCU8 DEP_DIR = strlit("build/.dep/");
SliceCU8 SRC_DIR = strlit("src/");

SliceCU8 SHLIB = M_SHLIB;
SliceCU8 STLIB = M_STLIB;

static int slice_cmp(const void* a, const void* b) {
    const SliceCU8* sa = (const SliceCU8*)a;
    const SliceCU8* sb = (const SliceCU8*)b;

    usize min_len = sa->len < sb->len ? sa->len : sb->len;
    int res = memcmp(sa->ptr, sb->ptr, min_len);

    if (res == 0) {
        return (sa->len > sb->len) - (sa->len < sb->len);
    }
    return res;
}

static inline u8 hex_char(u8 v) { return v < 10 ? '0' + v : 'A' + (v - 10); }

static usize mangle_cli_args(SliceCU8* args, usize num_args, u8* out_buf,
                             usize out_cap) {
    if (num_args == 0) {
        if (out_cap >= 7) {
            memcpy(out_buf, "default", 7);
            return 7;
        }
        return 0;
    }

    qsort(args, num_args, sizeof(SliceCU8), slice_cmp);

    usize pos = 0;
    for (usize i = 0; i < num_args; i++) {
        SliceCU8 arg = args[i];

        for (usize j = 0; j < arg.len; j++) {
            if (pos >= out_cap) return pos;  // Safety bound

            u8 c = arg.ptr[j];

            bool is_safe = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                           (c >= '0' && c <= '9') || c == '.' || c == '-';

            if (is_safe) {
                out_buf[pos++] = c;
            } else {
                if (pos + 3 > out_cap)
                    return pos;  // Make sure we have room for 3 chars
                out_buf[pos++] = '%';
                out_buf[pos++] = hex_char((c >> 4) & 0x0F);
                out_buf[pos++] = hex_char(c & 0x0F);
            }
        }

        if (i < num_args - 1) {
            if (pos < out_cap) out_buf[pos++] = '_';
        }
    }

    return pos;
}

ArgvBuilder cflags;
ArgvBuilder ldflags;
ArgvBuilder aflags;

CompileCommands ccmds;
ArenaAlloc arena;

int builder(const SliceCU8* sources, usize num_sources, SliceCU8 output,
            u32 thread_idx) {
    (void)thread_idx;
    zmm_fs_create_parent_path(output);

    bool is_so_link = zmm_p_has_ext(output, strlit(SO_EXT));
    bool is_st_link = zmm_p_has_ext(output, strlit(".a"));
    bool is_exe_link = zmm_p_has_ext(output, strlit(".out"));
    bool is_comp = !(is_so_link || is_st_link || is_exe_link);

    if (is_comp && num_sources != 1) {
        zmm_printf("More than one source for an object file..? aborting\n");
        return 1;
    }

    u8 cmd_buf[4096];
    u8 arg_ptr_buf[512];

    ArgvBuilder cmd;
    zmm_argv_initbuf(&cmd, cmd_buf, sizeof(cmd_buf), arg_ptr_buf,
                     sizeof(arg_ptr_buf));

    if (is_st_link) {
        zmm_argv_append(&cmd, AR);
        zmm_argv_appendz(&cmd, ARFLAGS);

        for (usize i = 0; i < num_sources; i++) {
            zmm_argv_append(&cmd, sources[i]);
        }
    } else {
        zmm_argv_append(&cmd, CC);

        if (is_comp) {
            zmm_argv_append_argv(&cmd, &cflags);

            u8 dep_path_buf[4096];
            u8* dep_heap = NULL;
            SliceU8 depfile_path =
                zmm_p_join_any(slicearr(SliceCU8,                     //
                                        DEP_DIR,                      //
                                        zmm_p_strip_ext(sources[0]),  //
                                        strlit(".d"),                 //
                                        NullSliceCU8                  //
                                        ),
                               dep_path_buf, sizeof(dep_path_buf), &dep_heap);

            zmm_fs_create_parent_path(slice_cast(SliceCU8, depfile_path));

            zmm_argv_append(&cmd, strlit("-MMD"));
            zmm_argv_append(&cmd, strlit("-MP"));
            zmm_argv_append(&cmd, strlit("-MF"));
            zmm_argv_append(&cmd, slice_cast(SliceCU8, depfile_path));
            zmm_argv_append(&cmd, strlit("-c"));

            if (dep_heap) free(dep_heap);
        }

        zmm_argv_append_argv(&cmd, &aflags);

        for (usize i = 0; i < num_sources; i++) {
            zmm_argv_append(&cmd, sources[i]);
        }

        if (is_so_link || is_exe_link) {
            zmm_argv_append_argv(&cmd, &ldflags);

            if (is_so_link) {
                zmm_argv_append(&cmd, strlit("-shared"));
            }

            if (is_so_link || is_st_link) {
                zmm_argv_appendz(&cmd, LDFLAGS_LIB);
            } else {
                zmm_argv_appendz(&cmd, LDFLAGS_EXE);
            }
        }

        zmm_argv_append(&cmd, strlit("-o"));
        zmm_argv_append(&cmd, output);

        if (is_comp) {
            zmm_cc_append(&ccmds, sources[0], cmd.flat, cmd.num_args);
        }
    }

    ChildTerm status = zmm_sys_exec_print(cmd.argv, cmd.num_args);
    zmm_argv_free(&cmd);

    return status.type == TERM_EXITED ? status.code : -1;
}

int main(int argc, char** argv) {
    // change cwd to project root
    zmm_fs_change_cwd(strlit("../"));

    Argv args;
    if (zmm_args_init(&args, argc, argv) != 0) {
        return 1;
    }

    if (zmm_args_contains(&args, strlit("clean"))) {
        usize i = 0;
        while (TESTING_EXES[i].ptr) zmm_fs_delete_file(TESTING_EXES[i++]);

        zmm_fs_delete_file(SHLIB);
        zmm_fs_delete_file(STLIB);

        zmm_fs_delete_tree(OBJ_DIR);
        zmm_fs_delete_tree(DEP_DIR);
        zmm_args_free(&args);
        return 0;
    }

    zmm_argv_initcap(&cflags, 2048, 256);
    zmm_argv_initcap(&ldflags, 2048, 256);
    zmm_argv_initcap(&aflags, 2048, 256);

    zmm_argv_appendz(&cflags, CFLAGS_BASE);
    zmm_argv_appendz(&ldflags, LDFLAGS_BASE);
    zmm_argv_appendz(&aflags, AFLAGS_BASE);

    // Apply optimization / debug flags
    if (zmm_args_contains(&args, strlit("-ffast"))) {
        zmm_argv_appendz(&cflags, CFLAGS_FAST);
    } else {
        zmm_argv_appendz(&cflags, CFLAGS_NO_FAST);
    }

    if (zmm_args_contains(&args, strlit("-fno-debug"))) {
        zmm_argv_appendz(&aflags, AFLAGS_NODEBUG);
    } else {
        zmm_argv_appendz(&aflags, AFLAGS_NO_NODEBUG);
    }

    // Apply sanitizers
    if (zmm_args_contains(&args, strlit("-fsan"))) {
        zmm_argv_appendz(&aflags, AFLAGS_SAN);
    } else {
        zmm_argv_appendz(&aflags, AFLAGS_NO_SAN);
    }

    // mangle args
    u8 mangled_args[255];
    usize mangled_args_len = mangle_cli_args(
        args.args + 1, args.num_args - 1, mangled_args, sizeof(mangled_args));

    SliceCU8 mangled_obj_dir = slice_cast(
        SliceCU8, zmm_p_join_any(slicearr(SliceCU8, OBJ_DIR,
                                          (SliceCU8){.ptr = mangled_args,
                                                     .len = mangled_args_len},
                                          NullSliceCU8),
                                 NULL, 0, NULL));

    // discover source files
    arr(SliceU8) lib_srcs = arrinit;
    zmm_fs_find(
        &lib_srcs, slicearr(SliceCU8, SRC_DIR, NullSliceCU8),
        (FindOpts){.depth = FD_RECURSIVE,
                   .exts = slicearr(SliceCU8, strlit(".c"), NullSliceCU8),
                   .ignore_hidden = true,
                   .kinds = (FindKind[]){FK_FILE, 0}});

    // create DAG
    BuildGraph bg;
    zmm_arena_init(&arena);
    zmm_bg_init(&bg, &arena);

    usize n_libs = 0;
    while (LIBS[n_libs++].ptr) (void)0;

    TargetBuilder* lib_tgs =
        zmm_arena_alloc(&arena, n_libs * sizeof(TargetBuilder));

    for (usize i = 0; i < n_libs; i++) {
        zmm_tg_init_out(&lib_tgs[i], &bg, LIBS[i]);
    }

    arr(SliceU8) deps = arrinit;

    for (usize i = 0; i < arrlenu(lib_srcs); i++) {
        SliceCU8 src = (SliceCU8){lib_srcs[i].ptr, lib_srcs[i].len};

        u8 dep_path_buf[4096];
        u8* dep_heap = NULL;
        SliceU8 depfile_path =
            zmm_p_join_any(slicearr(SliceCU8,              //
                                    DEP_DIR,               //
                                    zmm_p_strip_ext(src),  //
                                    strlit(".d"),          //
                                    NullSliceCU8           //
                                    ),
                           dep_path_buf, sizeof(dep_path_buf), &dep_heap);

        zmm_dep_parse(&deps, (SliceCU8){depfile_path.ptr, depfile_path.len});

        u8 obj_path_buf[4096];
        u8* obj_heap = NULL;
        SliceU8 objfile_path =
            zmm_p_join_any(slicearr(SliceCU8,              //
                                    mangled_obj_dir,       //
                                    zmm_p_strip_ext(src),  //
                                    strlit(".o"),          //
                                    NullSliceCU8           //
                                    ),
                           obj_path_buf, sizeof(obj_path_buf), &obj_heap);

        SliceCU8 arena_obj = {
            zmm_arena_dupe(&arena, objfile_path.ptr, objfile_path.len),
            objfile_path.len};

        zmm_bg_add(&bg, &src, 1, arena_obj, (SliceCU8*)deps, arrlenu(deps));

        for (usize j = 0; j < n_libs; j++) {
            zmm_tg_add_src_nc(&lib_tgs[j], &arena_obj, 1);
        }

        if (dep_heap) free(dep_heap);
        if (obj_heap) free(obj_heap);

        for (usize j = 0; j < arrlenu(deps); j++) {
            free(deps[j].ptr);
        }
        arrsetlen(deps, 0);
    }

    for (usize i = 0; TESTING_EXES[i].ptr != NULL; i++) {
        SliceCU8 src = TESTING_SRCS[i];
        SliceCU8 exe = TESTING_EXES[i];

        u8 dep_path_buf[4096];
        u8* dep_heap = NULL;
        SliceU8 depfile_path =
            zmm_p_join_any(slicearr(SliceCU8,              //
                                    DEP_DIR,               //
                                    zmm_p_strip_ext(src),  //
                                    strlit(".d"),          //
                                    NullSliceCU8           //
                                    ),
                           dep_path_buf, sizeof(dep_path_buf), &dep_heap);

        zmm_dep_parse(&deps, (SliceCU8){depfile_path.ptr, depfile_path.len});

        u8 obj_path_buf[4096];
        u8* obj_heap = NULL;
        SliceCU8 objfile_path = slice_cast(
            SliceCU8,
            zmm_p_join_any(slicearr(SliceCU8,              //
                                    mangled_obj_dir,       //
                                    zmm_p_strip_ext(src),  //
                                    strlit(".o"),          //
                                    NullSliceCU8           //
                                    ),
                           obj_path_buf, sizeof(obj_path_buf), &obj_heap));

        zmm_bg_add(&bg, &src, 1, slice_cast(SliceCU8, objfile_path),
                   (SliceCU8*)deps, arrlenu(deps));

        zmm_bg_add(&bg, &src, 1, objfile_path, (SliceCU8*)deps, arrlenu(deps));

        TargetBuilder test_exe_tg;

        zmm_tg_init_out(&test_exe_tg, &bg, exe);

        zmm_tg_add_src(&test_exe_tg, &objfile_path, 1);

        zmm_tg_add_dep_nc(&test_exe_tg, LIBS, n_libs);

        if (dep_heap) free(dep_heap);
        if (obj_heap) free(obj_heap);
        for (usize j = 0; j < arrlenu(deps); j++) {
            free(deps[j].ptr);
        }
        arrsetlen(deps, 0);
    }

    arrfree(deps);

    // build
    bool build_test_exes = true;
    if (zmm_args_contains(&args, strlit("lib"))) {
        build_test_exes = false;
    }

    zmm_cc_init_parse(&ccmds, &arena, CCMDS_FILE);

    arr(SliceCU8) to_build = arrinit;

    for (usize i = 0; i < n_libs; i++) {
        arrpush(to_build, LIBS[i]);
    }

    if (build_test_exes) {
        usize i = 0;
        while (TESTING_EXES[i].ptr) {
            arrpush(to_build, TESTING_EXES[i++]);
        }
    }

    u64 start = zmm_time_us();

    int exit_code = zmm_bg_build(&bg, to_build, arrlenu(to_build), builder);

    u64 end = zmm_time_us();

    arrfree(to_build);

    zmm_printf("=> Build finished in %.2fms\n", (end - start) / 1000.f);

    zmm_cc_write(&ccmds, CCMDS_FILE);

    zmm_arena_free(&arena);
    slicearr_free(lib_srcs);
    zmm_args_free(&args);
    zmm_bg_free(&bg);
    zmm_cc_free(&ccmds);

    return exit_code;
}
