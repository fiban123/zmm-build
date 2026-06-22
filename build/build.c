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

#include <zmm/zmm.h>

const StringView BASE_CFLAGS[] = {
    strv("-std=gnu23"), strv("-fPIC"),     strv("-fdiagnostics-color=always"),
    strv("-Iinclude/"), strv("-Iextern/"), strv("-DBUILDING_SO"),
};

const StringView FAST_CFLAGS[] = {strv("-O3")};

const StringView DEBUG_CFLAGS[] = {strv("-O0"), strv("-ggdb3")};

const StringView SAN_FLAGS[] = {strv("-fsanitize=address"),
                                strv("-fsanitize=undefined")};

ArgvBuilder cflags;
ArgvBuilder ldflags;

const StringView so_libs[] = {strv("-lcpu_features")};
const StringView exe_libs[] = {strv("-lzmm")};

StringView cc = strv("clang");

StringView test_src = strv("test/main.c");
StringView test_out = strv("build/test.out");

StringView cli_src = strv("cli/main.c");
StringView cli_out = strv("build/cli.out");

StringView so_out = strv("build/libzmm.so");

StringView ccmds_file = strv("compile_commands.json");

StringView obj_dir = strv("build/.obj/");
StringView dep_dir = strv("build/.dep/");
StringView src_dir = strv("src/");

CompileCommands ccmds;
StringView cwd;

int builder(const StringView* sources, usize num_sources, StringView output) {
    zmm_fs_create_parent_path(output);

    bool is_exe = zmm_p_has_ext(output, strv(".out"));
    bool is_so = zmm_p_has_ext(output, strv(".so"));
    bool is_obj = !is_exe && !is_so;

    ArgvBuilder cmd;
    zmm_argvb_initcap(&cmd, 4096, 512);

    zmm_argvb_append(&cmd, cc);

    if (!is_so) {
        zmm_argvb_append_argv(&cmd, (const char* const*)cflags.argv,
                              cflags.argv_len);
    }

    if (is_obj) {
        String depfile_path =
            zmm_p_join(dep_dir, zmm_p_strip_ext(sources[0]), strv(".d"));

        zmm_fs_create_parent_path(zmm_str_stov(depfile_path));

        zmm_argvb_append(&cmd, strv("-MMD"));
        zmm_argvb_append(&cmd, strv("-MP"));
        zmm_argvb_append(&cmd, strv("-MF"));
        zmm_argvb_append(&cmd, zmm_str_stov(depfile_path));
        zmm_argvb_append(&cmd, strv("-c"));

        free(depfile_path.ptr);
    }

    for (usize i = 0; i < num_sources; i++) {
        zmm_argvb_append(&cmd, sources[i]);
    }

    if (is_exe || is_so) {
        zmm_argvb_append_argv(&cmd, (const char* const*)ldflags.argv,
                              ldflags.argv_len);

        if (is_so) {
            zmm_argvb_append(&cmd, strv("-shared"));
            zmm_argvb_append_n(&cmd, so_libs,
                               sizeof(so_libs) / sizeof(so_libs[0]));
        }

        if (is_exe) {
            zmm_argvb_append_n(&cmd, exe_libs,
                               sizeof(exe_libs) / sizeof(exe_libs[0]));
        }
    }

    zmm_argvb_append(&cmd, strv("-o"));
    zmm_argvb_append(&cmd, output);

    if (is_obj && num_sources == 1) {
        zmm_cc_append(&ccmds, cwd, sources[0], cmd.buf, cmd.argv_len);
    }

    ChildTerm status = zmm_sys_exec_print(cmd.argv, cmd.argv_len);
    zmm_argvb_free(&cmd);

    return zmm_sys_term_code(status);
}

int main(int argc, char** argv) {
    // change cwd to project root
    zmm_fs_change_cwd(strv("../"));

    bool is_clean = false;
    bool is_fast = false;
    bool is_san = false;

    for (int i = 1; i < argc; i++) {
        StringView arg = zmm_str_ctov(argv[i]);
        if (zmm_str_eq(arg, strv("clean"))) {
            is_clean = true;
        } else if (zmm_str_eq(arg, strv("-ffast"))) {
            is_fast = true;
        } else if (zmm_str_eq(arg, strv("-fsan"))) {
            is_san = true;
        }
    }

    if (is_clean) {
        zmm_fs_delete_file(so_out);
        zmm_fs_delete_file(cli_out);
        zmm_fs_delete_file(test_out);
        zmm_fs_delete_tree(obj_dir);
        zmm_fs_delete_tree(dep_dir);
        return 0;
    }

    zmm_argvb_initcap(&cflags, 2048, 256);
    zmm_argvb_initcap(&ldflags, 2048, 256);

    // Apply base flags
    zmm_argvb_append_n(&cflags, BASE_CFLAGS,
                       sizeof(BASE_CFLAGS) / sizeof(BASE_CFLAGS[0]));

    // Apply optimization / debug flags
    if (is_fast) {
        zmm_argvb_append_n(&cflags, FAST_CFLAGS,
                           sizeof(FAST_CFLAGS) / sizeof(FAST_CFLAGS[0]));
    } else {
        zmm_argvb_append_n(&cflags, DEBUG_CFLAGS,
                           sizeof(DEBUG_CFLAGS) / sizeof(DEBUG_CFLAGS[0]));
    }

    // Apply sanitizers
    if (is_san) {
        zmm_argvb_append_n(&cflags, SAN_FLAGS,
                           sizeof(SAN_FLAGS) / sizeof(SAN_FLAGS[0]));
        zmm_argvb_append_n(&ldflags, SAN_FLAGS,
                           sizeof(SAN_FLAGS) / sizeof(SAN_FLAGS[0]));
    }

    // discover source files
    vec(String) lib_srcs = NULL;
    zmm_fs_find(&lib_srcs, src_dir,
                (FindOpts){.depth = FD_RECURSIVE,
                           .kind = FK_FILE,
                           .ext = strv(".c"),
                           .ignore_hidden = true});

    // create DAG
    BuildGraph bg;
    zmm_bg_init(&bg);

    TargetBuilder lib_tg;
    zmm_tg_init(&lib_tg, &bg, so_out);
    zmm_tg_set_builder(&lib_tg, builder);

    for (usize i = 0; i < vecsize(lib_srcs); i++) {
        StringView src = zmm_str_stov(lib_srcs[i]);
        String objfile_path =
            zmm_p_join(obj_dir, zmm_p_strip_ext(src), strv(".o"));
        StringView obj = zmm_str_stov(objfile_path);

        String depfile_path =
            zmm_p_join(dep_dir, zmm_p_strip_ext(src), strv(".d"));
        vec(String) deps = NULL;
        zmm_dep_parse(&deps, zmm_str_stov(depfile_path));

        usize num_deps = vecsize(deps);
        StringView* dep_views = NULL;
        if (num_deps > 0) {
            vecreserve(dep_views, num_deps);
            for (usize j = 0; j < num_deps; j++) {
                vecpush(dep_views, zmm_str_stov(deps[j]));
            }
        }

        zmm_bg_add(&bg, &src, 1, obj, dep_views, num_deps, builder, false);
        zmm_tg_add_src(&lib_tg, &obj, 1);

        vecfree(dep_views);
        for (usize j = 0; j < num_deps; j++) {
            free(deps[j].ptr);
        }
        vecfree(deps);
        free(depfile_path.ptr);
        free(objfile_path.ptr);
    }

    TargetBuilder cli_tg, test_tg;
    zmm_tg_init(&cli_tg, &bg, cli_out);
    zmm_tg_init(&test_tg, &bg, test_out);

    zmm_tg_set_builder(&cli_tg, builder);
    zmm_tg_set_builder(&test_tg, builder);

    zmm_tg_add_src(&cli_tg, &cli_src, 1);
    zmm_tg_add_src(&test_tg, &test_src, 1);

    zmm_tg_add_dep(&cli_tg, &so_out, 1);
    zmm_tg_add_dep(&test_tg, &so_out, 1);

    zmm_cc_init(&ccmds, ccmds_file);
    String cwd_str = zmm_fs_abs_cwd();
    cwd = zmm_str_stov(cwd_str);

    const StringView to_build[] = {so_out, test_out, cli_out};

    u64 start = zmm_time_us();

    int exit_code = 0;
    if (zmm_bg_prepare(&bg, to_build, 3) != 0) {
        exit_code = 1;
    } else {
        exit_code = zmm_bg_exec(&bg);
    }

    u64 end = zmm_time_us();

    zmm_printf("=> Build finished in %.2fms\n", (end - start) / 1000.);

    zmm_cc_finish(&ccmds);
    free(cwd_str.ptr);

    for (usize i = 0; i < vecsize(lib_srcs); i++) {
        free(lib_srcs[i].ptr);
    }
    vecfree(lib_srcs);

    zmm_argvb_free(&cflags);
    zmm_argvb_free(&ldflags);
    zmm_bg_free(&bg);

    return exit_code;
}
