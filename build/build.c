#include <zmm/zmm.h>

#include "slice.h"

// *** config ***

// Combined base compilation flags
#define CONF_CFLAGS                        \
    {strlit("-std=gnu23"),                 \
     strlit("-fPIC"),                      \
     strlit("-fdiagnostics-color=always"), \
     strlit("-Iinclude/"),                 \
     strlit("-Iextern/"),                  \
     strlit("-DBUILDING_SO"),              \
     NullSliceCU8}

// -ffast these flags are added when -ffast is used
#define CONF_CFLAGS_FAST {strlit("-O3"), NullSliceCU8}
// not -ffast these flags are added when -ffast is not used
#define CONF_CFLAGS_DEBUG {strlit("-O0"), strlit("-ggdb3"), NullSliceCU8}

// -fsan these flags are added when -fsan is used
#define CONF_FLAGS_SAN \
    {strlit("-fsanitize=address"), strlit("-fsanitize=undefined"), NullSliceCU8}

ArgvBuilder cflags;
ArgvBuilder ldflags;

SliceCU8 so_libs[] = {strlit("-lcpu_features"), NullSliceCU8};
SliceCU8 exe_libs[] = {strlit("-lzmm"), NullSliceCU8};

SliceCU8 cc = strlit("clang");

SliceCU8 test_src = strlit("test/main.c");
SliceCU8 test_out = strlit("build/test.out");

SliceCU8 cli_src = strlit("cli/main.c");
SliceCU8 cli_out = strlit("build/cli.out");

SliceCU8 so_out = strlit("build/libzmm.so");

SliceCU8 ccmds_file = strlit("compile_commands.json");

SliceCU8 obj_dir = strlit("build/.obj/");
SliceCU8 dep_dir = strlit("build/.dep/");
SliceCU8 src_dir = strlit("src/");

CompileCommands ccmds;
ArenaAlloc arena;

int builder(const SliceCU8* sources, usize num_sources, SliceCU8 output,
            u32 thread_idx) {
    (void)thread_idx;
    zmm_fs_create_parent_path(output);

    bool is_exe = zmm_p_has_ext(output, strlit(".out"));
    bool is_so = zmm_p_has_ext(output, strlit(".so"));
    bool is_obj = !is_exe && !is_so;

    u8 cmd_buf[4096];
    u8 arg_ptr_buf[512];

    ArgvBuilder cmd;
    zmm_argv_initbuf(&cmd, cmd_buf, sizeof(cmd_buf), arg_ptr_buf,
                     sizeof(arg_ptr_buf));

    zmm_argv_append(&cmd, cc);

    if (!is_so) {
        zmm_argv_append_argv(&cmd, &cflags);
    }

    if (is_obj) {
        u8 dep_path_buf[4096];
        u8* dep_heap = NULL;
        SliceU8 depfile_path =
            zmm_p_join_any(slicearr(SliceCU8,                     //
                                    strlit("build/.dep/"),        //
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

    for (usize i = 0; i < num_sources; i++) {
        zmm_argv_append(&cmd, sources[i]);
    }

    if (is_exe || is_so) {
        zmm_argv_append_argv(&cmd, &ldflags);

        if (is_so) {
            zmm_argv_append(&cmd, strlit("-shared"));
            zmm_argv_appendz(&cmd, so_libs);
        }

        if (is_exe) {
            zmm_argv_appendz(&cmd, exe_libs);
        }
    }

    zmm_argv_append(&cmd, strlit("-o"));
    zmm_argv_append(&cmd, output);

    if (is_obj && num_sources == 1) {
        zmm_cc_append(&ccmds, sources[0], cmd.flat, cmd.num_args);
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
        zmm_fs_delete_file(so_out);
        zmm_fs_delete_file(cli_out);
        zmm_fs_delete_file(test_out);
        zmm_fs_delete_tree(obj_dir);
        zmm_fs_delete_tree(dep_dir);
        zmm_args_free(&args);
        return 0;
    }

    zmm_argv_initcap(&cflags, 2048, 256);
    zmm_argv_initcap(&ldflags, 2048, 256);

    // Apply base flags
    SliceCU8 base_cflags[] = CONF_CFLAGS;
    zmm_argv_appendz(&cflags, base_cflags);

    // Apply optimization / debug flags
    if (zmm_args_contains(&args, strlit("-ffast"))) {
        SliceCU8 fast_flags[] = CONF_CFLAGS_FAST;
        zmm_argv_appendz(&cflags, fast_flags);
    } else {
        SliceCU8 debug_flags[] = CONF_CFLAGS_DEBUG;
        zmm_argv_appendz(&cflags, debug_flags);
    }

    // Apply sanitizers
    if (zmm_args_contains(&args, strlit("-fsan"))) {
        SliceCU8 san_flags[] = CONF_FLAGS_SAN;
        zmm_argv_appendz(&cflags, san_flags);
        zmm_argv_appendz(&ldflags, san_flags);
    }

    // discover source files
    arr(SliceU8) lib_srcs = arrinit;
    zmm_fs_find(
        &lib_srcs, slicearr(SliceCU8, src_dir, NullSliceCU8),
        (FindOpts){.depth = FD_RECURSIVE,
                   .exts = slicearr(SliceCU8, strlit(".c"), NullSliceCU8),
                   .ignore_hidden = true,
                   .kinds = (FindKind[]){FK_FILE, 0}});

    // create DAG
    BuildGraph bg;
    zmm_arena_init(&arena);
    zmm_bg_init(&bg, &arena);

    TargetBuilder lib_tg;
    zmm_tg_init_out(&lib_tg, &bg, so_out);

    arr(SliceU8) deps = arrinit;

    for (usize i = 0; i < arrlenu(lib_srcs); i++) {
        SliceCU8 src = (SliceCU8){lib_srcs[i].ptr, lib_srcs[i].len};

        u8 dep_path_buf[4096];
        u8* dep_heap = NULL;
        SliceU8 depfile_path =
            zmm_p_join_any(slicearr(SliceCU8,              //
                                    dep_dir,               //
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
                                    obj_dir,               //
                                    zmm_p_strip_ext(src),  //
                                    strlit(".o"),          //
                                    NullSliceCU8           //
                                    ),
                           obj_path_buf, sizeof(obj_path_buf), &obj_heap);

        SliceCU8 arena_obj = {
            zmm_arena_dupe(&arena, objfile_path.ptr, objfile_path.len),
            objfile_path.len};

        zmm_bg_add(&bg, &src, 1, arena_obj, (SliceCU8*)deps, arrlenu(deps));
        zmm_tg_add_src_nc(&lib_tg, &arena_obj, 1);

        if (dep_heap) free(dep_heap);
        if (obj_heap) free(obj_heap);
        for (usize j = 0; j < arrlenu(deps); j++) {
            free(deps[j].ptr);
        }
        arrsetlen(deps, 0);
    }

    arrfree(deps);

    TargetBuilder cli_tg, test_tg;
    zmm_tg_init_out(&cli_tg, &bg, cli_out);
    zmm_tg_init_out(&test_tg, &bg, test_out);

    zmm_tg_add_src_nc(&cli_tg, &cli_src, 1);
    zmm_tg_add_src_nc(&test_tg, &test_src, 1);

    zmm_tg_add_dep_nc(&cli_tg, &so_out, 1);
    zmm_tg_add_dep_nc(&test_tg, &so_out, 1);

    zmm_cc_init_parse(&ccmds, &arena, ccmds_file);

    SliceCU8 to_build[] = {so_out, test_out, cli_out};

    u64 start = zmm_time_us();

    int exit_code = zmm_bg_build(&bg, to_build, 3, builder);

    u64 end = zmm_time_us();

    zmm_printf("=> Build finished in %.2fms\n", (end - start) / 1000.);

    zmm_cc_write(&ccmds, ccmds_file);

    zmm_arena_free(&arena);
    slicearr_free(lib_srcs);
    zmm_args_free(&args);
    zmm_bg_free(&bg);
    zmm_cc_free(&ccmds);

    return exit_code;
}
