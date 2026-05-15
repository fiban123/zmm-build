#include <zmm/zmm.h>

// *** config ***

// compilation base flags
SliceCU8 CFLAGS_BASE[] = {strlit("-std=gnu23"),
                          strlit("-fdiagnostics-color=always"),
                          strlit("-Iinclude/"), NullSliceCU8};

// linking base flags (link pthread as an example)
SliceCU8 LDFLAGS_BASE[] = {strlit("-lpthread"), NullSliceCU8};

// all base flags (compilation + link)
SliceCU8 AFLAGS_BASE[] = {NullSliceCU8};

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

SliceCU8 EXE_OUT = strlit("build/exe.out");

SliceCU8 CCMDS_FILE = strlit("compile_commands.json");

SliceCU8 OBJ_DIR = strlit("build/.obj/");
SliceCU8 DEP_DIR = strlit("build/.dep/");
SliceCU8 SRC_DIR = strlit("src/");

ArgvBuilder cflags;
ArgvBuilder ldflags;
ArgvBuilder aflags;

CompileCommands ccmds;
ArenaAlloc arena;

int builder(const SliceCU8* sources, usize num_sources, SliceCU8 output,
            u32 thread_idx) {
    (void)thread_idx;
    zmm_fs_create_parent_path(output);

    bool is_link = zmm_p_has_ext(output, strlit(".out"));
    bool is_comp = !is_link;

    u8 cmd_buf[4096];
    u8 arg_ptr_buf[512];

    ArgvBuilder cmd;
    zmm_argv_initbuf(&cmd, cmd_buf, sizeof(cmd_buf), arg_ptr_buf,
                     sizeof(arg_ptr_buf));

    zmm_argv_append(&cmd, CC);

    if (is_comp) {
        zmm_argv_append_argv(&cmd, &cflags);

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

    zmm_argv_append_argv(&cmd, &aflags);

    for (usize i = 0; i < num_sources; i++) {
        zmm_argv_append(&cmd, sources[i]);
    }

    if (is_link) {
        zmm_argv_append_argv(&cmd, &ldflags);
    }

    zmm_argv_append(&cmd, strlit("-o"));
    zmm_argv_append(&cmd, output);

    if (is_comp && num_sources == 1) {
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
        zmm_fs_delete_file(EXE_OUT);
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

    // Apply sanitizers
    if (zmm_args_contains(&args, strlit("-fsan"))) {
        zmm_argv_appendz(&aflags, AFLAGS_SAN);
    } else {
        zmm_argv_appendz(&aflags, AFLAGS_NO_SAN);
    }

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

    TargetBuilder exe_tg;
    zmm_tg_init_out(&exe_tg, &bg, EXE_OUT);

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
                                    OBJ_DIR,               //
                                    zmm_p_strip_ext(src),  //
                                    strlit(".o"),          //
                                    NullSliceCU8           //
                                    ),
                           obj_path_buf, sizeof(obj_path_buf), &obj_heap);

        SliceCU8 arena_obj = {
            zmm_arena_dupe(&arena, objfile_path.ptr, objfile_path.len),
            objfile_path.len};

        zmm_bg_add(&bg, &src, 1, arena_obj, (SliceCU8*)deps, arrlenu(deps));

        zmm_tg_add_src_nc(&exe_tg, &arena_obj, 1);

        if (dep_heap) free(dep_heap);
        if (obj_heap) free(obj_heap);

        for (usize j = 0; j < arrlenu(deps); j++) {
            free(deps[j].ptr);
        }
        arrsetlen(deps, 0);
    }

    zmm_cc_init_parse(&ccmds, &arena, CCMDS_FILE);

    u64 start = zmm_time_us();

    int exit_code = 0;
    if (zmm_bg_build(&bg, &EXE_OUT, 1, builder)) {
        exit_code = 1;
    }

    u64 end = zmm_time_us();

    zmm_printf("=> Build finished in %.2fms\n", (end - start) / 1000.f);

    zmm_cc_write(&ccmds, CCMDS_FILE);

    zmm_arena_free(&arena);
    slicearr_free(lib_srcs);
    zmm_args_free(&args);
    zmm_bg_free(&bg);
    zmm_cc_free(&ccmds);

    return exit_code;
}
