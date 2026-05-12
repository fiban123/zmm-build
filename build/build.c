#include <zmm/cc.h>
#include <zmm/slice.h>
#include <zmm/zmm.h>

ArgvBuilder cflags;
ArgvBuilder ppflags;
ArgvBuilder ldflags;

SliceCU8 so_libs[] = {strlit("-lcpu_features"), NullSliceCU8};
SliceCU8 exe_libs[] = {strlit("-lzmm"), NullSliceCU8};

SliceCU8 cc = strlit("clang");

SliceCU8 test_src = strlit("test/main.c");
SliceCU8 test_out = strlit("test/test.out");

SliceCU8 cli_src = strlit("cli/main.c");
SliceCU8 cli_out = strlit("cli/cli.out");

SliceCU8 so_out = strlit("build/libzmm.so");

SliceCU8 ccmds_file = strlit("compile_commands.json");

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
        append_many_to_cmd(&cmd, cflags, arrlenu(cflags));
        append_many_to_cmd(&cmd, ppflags, arrlenu(ppflags));
    }

    if (is_obj) {
        zmm_argv_append(&cmd, strlit("-c"));
    }

    for (usize i = 0; i < num_sources; i++) {
        zmm_argv_append(&cmd, sources[i]);
    }

    if (is_exe || is_so) {
        append_many_to_cmd(&cmd, ldflags, arrlenu(ldflags));

        if (is_so) {
            zmm_argv_append(&cmd, strlit("-shared"));
            append_many_to_cmd_z(&cmd, so_libs);
        }

        if (is_exe) {
            append_many_to_cmd_z(&cmd, exe_libs);
        }
    }

    zmm_argv_append(&cmd, strlit("-o"));
    zmm_argv_append(&cmd, output);

    if (is_obj && num_sources == 1) {
        zmm_cc_append(&ccmds, &arena, sources[0], cmd.flat, cmd.num_args);
    }

    ExecStatus status = zmm_sys_exec_print(cmd.argv, cmd.num_args);
    zmm_argv_free(&cmd);

    return status.term.type == TERM_EXITED ? status.term.code : -1;
}

int main(int argc, char** argv) {
    // change cwd to project root
    zmm_fs_change_cwd(strlit("../"));

    Argv args;
    if (zmm_args_init(&args, argc, argv) != 0) {
        return 1;
    }

    if (zmm_args_contains(&args, strlit("clean"))) {
        zmm_fs_delete_file(strlit("build/libzmm.a"));
        zmm_fs_delete_file(strlit("build/libzmm.so"));
        zmm_fs_delete_file(strlit("build/cli.out"));
        zmm_fs_delete_file(strlit("build/test.out"));
        zmm_fs_delete_tree(strlit("build/.obj"));
        zmm_fs_delete_tree(strlit("build/.dep"));
        zmm_args_free(&args);
        return 0;
    }

    arrpush(cflags, strlit("-fPIC"));
    arrpush(cflags, strlit("-std=c23"));

    arrpush(ppflags, strlit("-Iinclude/"));
    arrpush(ppflags, strlit("-Iextern/"));

    if (zmm_args_contains(&args, strlit("-ffast"))) {
        arrpush(cflags, strlit("-O3"));
    } else {
        arrpush(cflags, strlit("-O0"));
        arrpush(cflags, strlit("-ggdb3"));
    }

    if (zmm_args_contains(&args, strlit("-fsan"))) {
        arrpush(cflags, strlit("-fsanitize=address"));
        arrpush(cflags, strlit("-fsanitize=undefined"));
        arrpush(ldflags, strlit("-fsanitize=address"));
        arrpush(ldflags, strlit("-fsanitize=undefined"));
    }

    // discover source files
    arr(SliceU8) lib_srcs = arrinit;
    zmm_fs_find(
        &lib_srcs, slicearr(SliceCU8, strlit("src/"), NullSliceCU8),
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
            zmm_p_join_any(slicearr(SliceCU8,               //
                                    strlit("build/.dep/"),  //
                                    zmm_p_strip_ext(src),   //
                                    strlit(".d"),           //
                                    NullSliceCU8            //
                                    ),
                           dep_path_buf, sizeof(dep_path_buf), &dep_heap);

        zmm_dep_parse(&deps, (SliceCU8){depfile_path.ptr, depfile_path.len});

        u8 obj_path_buf[4096];
        u8* obj_heap = NULL;
        SliceU8 objfile_path =
            zmm_p_join_any(slicearr(SliceCU8,               //
                                    strlit("build/.obj/"),  //
                                    zmm_p_strip_ext(src),   //
                                    strlit(".o"),           //
                                    NullSliceCU8            //
                                    ),
                           obj_path_buf, sizeof(obj_path_buf), &obj_heap);

        SliceCU8 arena_obj = {
            zmm_arena_dupe(&arena, objfile_path.ptr, objfile_path.len),
            objfile_path.len};

        zmm_bg_add(&bg, &src, 1, arena_obj, (SliceCU8*)deps, arrlenu(deps));
        zmm_tg_add_src_nc(&lib_tg, &arena_obj, 1);

        if (dep_heap) free(dep_heap);
        if (obj_heap) free(obj_heap);
        arrsetlen(deps, 0);
    }

    slicearr_free(deps);

    TargetBuilder cli_tg, test_tg;
    zmm_tg_init_out(&cli_tg, &bg, cli_out);
    zmm_tg_init_out(&test_tg, &bg, test_out);

    zmm_tg_add_src_nc(&cli_tg, &cli_src, 1);
    zmm_tg_add_src_nc(&test_tg, &test_src, 1);

    zmm_tg_add_dep_nc(&cli_tg, &so_out, 1);
    zmm_tg_add_dep_nc(&test_tg, &so_out, 1);

    zmm_cc_parse(&ccmds, &arena, ccmds_file);

    u64 start = zmm_time_us();

    int exit_code = 0;
    if (zmm_bg_build(&bg, so_out, builder) ||
        zmm_bg_build(&bg, test_out, builder) ||
        zmm_bg_build(&bg, cli_out, builder)) {
        exit_code = 1;
    }

    u64 end = zmm_time_us();

    zmm_printf("Build finished in %u64ms\n", (end - start) / 1000);

    zmm_cc_write(&ccmds, ccmds_file);

    zmm_arena_free(&arena);
    slicearr_free(lib_srcs);
    zmm_args_free(&args);
    zmm_bg_free(&bg);
    zmm_cc_free(&ccmds);

    return exit_code;
}
