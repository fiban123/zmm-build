#include <stdio.h>
#include <zmm/arena.h>
#include <zmm/slice.h>
#include <zmm/zmm.h>

arr(SliceCU8) cflags = arrinit;
arr(SliceCU8) ldflags = arrinit;

int main(int argc, char** argv) {
    // change cwd to project root
    zmm_fs_change_cwd(strlit("../"));

    Argv args;
    zmm_arg_init(&args, argc, argv);

    if (zmm_arg_contains(&args, strlit("clean"))) {
        zmm_fs_delete_file(strlit("build/libzmm.a"));
        zmm_fs_delete_file(strlit("build/libzmm.so"));
        zmm_fs_delete_file(strlit("build/cli"));
        zmm_fs_delete_tree(strlit("build/.obj"));
        zmm_fs_delete_tree(strlit("build/.dep"));
        return 0;
    }

    if (zmm_arg_contains(&args, strlit("-ffast"))) {
        arrpush(cflags, strlit("-O3"));
    } else {
        arrpush(cflags, strlit("-O0"));
        arrpush(cflags, strlit("-ggdb3"));
    }

    // discover source files
    arr(SliceU8) lib_srcs = arrinit;
    arr(SliceU8) cli_srcs = arrinit;

    zmm_fs_find(&lib_srcs,  //
                slicearr(SliceCU8, strlit("src/"), NullSliceCU8),
                (FindOpts){
                    //
                    .depth = FD_RECURSIVE,
                    .exts = slicearr(SliceCU8, strlit(".c"), NullSliceCU8),
                    .ignore_hidden = true,
                    .kinds = (FindKind[]){FK_FILE, NULL}  //
                });

    zmm_fs_find(&cli_srcs,  //
                slicearr(SliceCU8, strlit("cli/"), NullSliceCU8),
                (FindOpts){
                    //
                    .depth = FD_RECURSIVE,
                    .exts = slicearr(SliceCU8, strlit(".c"), NullSliceCU8),
                    .ignore_hidden = true,
                    .kinds = (FindKind[]){FK_FILE, NULL}  //
                });

    // create DAG

    ArenaAlloc arena;
    zmm_arena_init(&arena);

    BuildGraph bg;
    zmm_builder_init(&bg);

    // transform to object- and depfile- paths

    for (usize i = 0; i < arrlenu(lib_srcs); i++) {
        u8 path_buf[4096];

        SliceCU8 src = slice_cast(SliceCU8, lib_srcs[i]);

        zmm_p_join_any(                      //
            slicearr(SliceCU8,               //
                     strlit("build/.obj/"),  //
                     zmm_p_strip_ext(src),   //
                     strlit(".o"),           //
                     NullSliceCU8            //
                     ),                      //
        )
    }
}
