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

#include <jsmn/jsmn.h>
#include <stb/stb_ds.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "args.h"
#include "arr.h"
#include "cc.h"
#include "num.h"
#include "print.h"
#include "slice.h"
#include "sys.h"

typedef struct {
    arr(SliceCU8) build_script_srcs;
    SliceCU8 build_script_exe;
    SliceCU8 build_script_compile_cmd;
} ZmmConfig;

static void zmmconfig_free(ZmmConfig* cfg) {
    for (ptrdiff_t i = 0; i < arrlen(cfg->build_script_srcs); ++i) {
        if (cfg->build_script_srcs[i].ptr) {
            free((void*)cfg->build_script_srcs[i].ptr);
        }
    }
    arrfree(cfg->build_script_srcs);

    // Free the standalone strings
    if (cfg->build_script_exe.ptr) {
        free((void*)cfg->build_script_exe.ptr);
    }
    if (cfg->build_script_compile_cmd.ptr) {
        free((void*)cfg->build_script_compile_cmd.ptr);
    }
}

/**
 * Resolves standard JSON escape sequences into a newly allocated slice.
 */
static SliceCU8 unescape_json_string(const u8* src, usize len) {
    // The unescaped string will never be longer than the escaped string.
    u8* dest = malloc(len);
    if (!dest) return (SliceCU8){0};

    usize d = 0;
    for (usize i = 0; i < len; i++) {
        if (src[i] == '\\' && i + 1 < len) {
            i++;
            switch (src[i]) {
                case '"':
                    dest[d++] = '"';
                    break;
                case '\\':
                    dest[d++] = '\\';
                    break;
                case '/':
                    dest[d++] = '/';
                    break;
                case 'b':
                    dest[d++] = '\b';
                    break;
                case 'f':
                    dest[d++] = '\f';
                    break;
                case 'n':
                    dest[d++] = '\n';
                    break;
                case 'r':
                    dest[d++] = '\r';
                    break;
                case 't':
                    dest[d++] = '\t';
                    break;
                default:
                    dest[d++] = src[i];
                    break;
            }
        } else {
            dest[d++] = src[i];
        }
    }

    return (SliceCU8){.ptr = dest, .len = d};
}

/**
 * Helper to compare a jsmn token string with a null-terminated C string.
 */
static bool json_token_eq(const char* json, jsmntok_t* tok, const char* s) {
    if (tok->type == JSMN_STRING) {
        int len = tok->end - tok->start;
        if (len == (int)strlen(s) && strncmp(json + tok->start, s, len) == 0) {
            return true;
        }
    }
    return false;
}

/**
 * Helper to skip a token and all of its nested children.
 * This relies on the fact that jsmn tokens sequentially bound their children.
 */
static int jsmn_skip(jsmntok_t* tokens, int count, int i) {
    if (i >= count) return i;
    int end = tokens[i].end;
    i++;
    while (i < count && tokens[i].start < end && tokens[i].start != 0) {
        i++;
    }
    return i;
}

/**
 * Parses the .zmm JSON string into a ZmmConfig struct.
 *
 * @param json_string The raw JSON string buffer.
 * @param json_len The length of the JSON string.
 * @param out_config Pointer to the config struct to populate.
 * @return 0 if successfully parsed, 1 otherwise.
 */
i32 parse_zmm_config(ZmmConfig* cfg, SliceCU8 jstr) {
    cfg->build_script_srcs = arrinit;
    cfg->build_script_exe = NullSliceCU8;
    cfg->build_script_compile_cmd = NullSliceCU8;

    jsmn_parser p;
    jsmn_init(&p);

    int tok_count = jsmn_parse(&p, (const char*)jstr.ptr, jstr.len, NULL, 0);
    if (tok_count < 0) return 1;

    jsmntok_t* t = malloc(sizeof(jsmntok_t) * tok_count);
    if (!t) return 1;

    jsmn_init(&p);
    int r = jsmn_parse(&p, (const char*)jstr.ptr, jstr.len, t, tok_count);

    i32 result = 0;

    if (r < 1 || t[0].type != JSMN_OBJECT) {
        result = 1;
        goto cleanup;
    }

    int i = 1;
    while (i < r) {
        if (t[i].type != JSMN_STRING) {
            i = jsmn_skip(t, r, i);
            continue;
        }

        if (json_token_eq((const char*)jstr.ptr, &t[i], "build-script-srcs")) {
            i++;
            if (i < r && t[i].type == JSMN_ARRAY) {
                int array_size = t[i].size;
                i++;
                for (int j = 0; j < array_size && i < r; j++) {
                    if (t[i].type == JSMN_STRING) {
                        // Unescape array elements
                        SliceCU8 src = unescape_json_string(
                            jstr.ptr + t[i].start,
                            (usize)(t[i].end - t[i].start));
                        arrpush(cfg->build_script_srcs, src);
                        i++;
                    } else {
                        i = jsmn_skip(t, r, i);
                    }
                }
            } else {
                i = jsmn_skip(t, r, i);
            }
        } else if (json_token_eq((const char*)jstr.ptr, &t[i],
                                 "build-script-exe")) {
            i++;
            if (i < r && t[i].type == JSMN_STRING) {
                // Unescape exe string
                cfg->build_script_exe = unescape_json_string(
                    jstr.ptr + t[i].start, (usize)(t[i].end - t[i].start));
            }
            i++;
        } else if (json_token_eq((const char*)jstr.ptr, &t[i],
                                 "build-script-compile-cmd")) {
            i++;
            if (i < r && t[i].type == JSMN_STRING) {
                // Unescape compile command string
                cfg->build_script_compile_cmd = unescape_json_string(
                    jstr.ptr + t[i].start, (usize)(t[i].end - t[i].start));
            }
            i++;
        } else {
            i++;
            i = jsmn_skip(t, r, i);
        }
    }

cleanup:
    free(t);
    return result;
}

// returns true if b is newer than a
static inline bool is_dirty_stat(const struct stat* a_stat,
                                 const struct stat* b_stat) {
    long a_sec = a_stat->st_mtime;
    long b_sec = b_stat->st_mtime;

#ifdef _WIN32
    // Windows/MinGW standard stat lacks nanosecond precision
    long a_nsec = 0;
    long b_nsec = 0;
#elif defined(__APPLE__)
    // macOS and BSD-based systems
    long a_nsec = a_stat->st_mtimespec.tv_nsec;
    long b_nsec = b_stat->st_mtimespec.tv_nsec;
#else
    // Linux and standard POSIX systems
    long a_nsec = a_stat->st_mtim.tv_nsec;
    long b_nsec = b_stat->st_mtim.tv_nsec;
#endif

    if (b_sec > a_sec) return true;
    if (a_sec == b_sec && b_nsec > a_nsec) return true;

    return false;
}

static bool is_dirty(SliceCU8 output, SliceCU8 const* inputs,
                     usize num_inputs) {
    if (num_inputs == 0) return true;
    char* out_path = slice_to_cstr(output);
    struct stat out_stat;
    int out_res = stat(out_path, &out_stat);
    free(out_path);

    if (out_res != 0) return true;

    for (usize i = 0; i < num_inputs; ++i) {
        char* in_path = slice_to_cstr(inputs[i]);
        struct stat in_stat;
        int b_res = stat(in_path, &in_stat);
        free(in_path);
        if (b_res != 0) return true;

        if (is_dirty_stat(&out_stat, &in_stat)) return true;
    }

    return false;
}

static u64 get_us(void) {
    struct timespec spec;
    timespec_get(&spec, TIME_UTC);

    return (spec.tv_sec * 1000000) + (spec.tv_nsec / 1000);
}

int main(int argc, char** argv) {
    FILE* cfg_file = fopen(".zmm", "rb");
    if (!cfg_file) {
        zmm_printf("No config file (.zmm) was found :(\n");
        return 1;
    }

    fseek(cfg_file, 0, SEEK_END);
    usize fsize = ftell(cfg_file);
    fseek(cfg_file, 0, SEEK_SET);

    SliceU8 cfg_str = {.ptr = malloc(fsize), .len = fsize};
    if (!cfg_str.ptr) {
        zmm_printf("Out of Memory (how big is your config file ..?)\n");
        fclose(cfg_file);
        return 1;
    }
    usize nread = fread(cfg_str.ptr, 1, fsize, cfg_file);
    fclose(cfg_file);  // Done with the file

    if (nread != fsize) {
        zmm_printf("Reading the config file from disk failed :(\n");
        free(cfg_str.ptr);
        return 1;
    }

    ZmmConfig cfg;
    int return_code = 0;

    i32 out = parse_zmm_config(&cfg, slice_cast(SliceCU8, cfg_str));

    if (out) {
        zmm_printf("Parsing the config file failed :(\n");
        return_code = 1;
        goto cleanup;
    }

    if (!cfg.build_script_srcs) {
        zmm_printf(
            "Warning: Config file does not contain sources "
            "(build-script-srcs)\n");
    }
    if (!cfg.build_script_exe.ptr) {
        zmm_printf(
            "Config file does not contain executable (build-script-exe)\n");
        return_code = 1;
        goto cleanup;
    }
    if (!cfg.build_script_compile_cmd.ptr) {
        zmm_printf(
            "Config file does not contain build command "
            "(build-script-compile-cmd)\n");
        return_code = 1;
        goto cleanup;
    }

    bool dirty = is_dirty(cfg.build_script_exe, cfg.build_script_srcs,
                          arrlenu(cfg.build_script_srcs));

    dirty |=
        is_dirty(cfg.build_script_exe, slicearr(SliceCU8, strlit(".zmm")), 1);

    if (dirty) {
        zmm_printf("=> Compiling build script...\n");
        u64 start = get_us();
        // run the builder command
        // to avoid complex escaping, we just use the shell
        // posix: sh -c "..."
        // win: cmd /c "..."
        u8 cmd_buf[1024];
        u8 cmd_ptrbuf[256];

        ArgvBuilder cmd;
        zmm_argv_initbuf(&cmd, cmd_buf, sizeof(cmd_buf), cmd_ptrbuf,
                         sizeof(cmd_ptrbuf));

#ifdef _WIN32
        zmm_argv_appendz(&cmd, slicearr(SliceCU8, strlit("cmd"), strlit("/c"),
                                        NullSliceCU8));
#else
        zmm_argv_appendz(
            &cmd, slicearr(SliceCU8, strlit("sh"), strlit("-c"), NullSliceCU8));
#endif

        zmm_argv_append(&cmd, cfg.build_script_compile_cmd);

        ChildTerm s = zmm_sys_exec_print(cmd.argv, cmd.num_args);

        zmm_argv_free(&cmd);

        if (s.type == TERM_ERROR) {
            zmm_printf("Failed to execute builder command :(\n");
            return_code = 1;
            goto cleanup;
        } else if (s.type == TERM_SIGNALED) {
            zmm_printf("Builder command forcibly terminated by OS\n");
            return_code = 1;
            goto cleanup;
        } else {
            if (s.code) {
                zmm_printf("Builder command failed :(\n");
                return_code = 1;
                goto cleanup;
            }
        }

        u64 end = get_us();

        zmm_printf("Build script compiled in %u64ms\n", (end - start) / 1000);
    }

    // execute the compiled build script
    u8 cmd_buf[1024];
    u8 cmd_ptrbuf[256];

    ArgvBuilder cmd;
    zmm_argv_initbuf(&cmd, cmd_buf, sizeof(cmd_buf), cmd_ptrbuf,
                     sizeof(cmd_ptrbuf));

#ifdef _WIN32
    zmm_argv_pappend(&cmd, strlit(".\\"));
#else
    zmm_argv_pappend(&cmd, strlit("./"));
#endif
    zmm_argv_pappend(&cmd, cfg.build_script_exe);
    zmm_argv_pfinish(&cmd);

    for (int i = 1; i < argc; i++) {
        zmm_argv_append(&cmd, (SliceCU8){.ptr = (const u8*)argv[i],
                                         .len = strlen(argv[i])});
    }

    ChildTerm s = zmm_sys_exec_redirect(cmd.argv, cmd.num_args);

    zmm_argv_free(&cmd);

    if (s.type == TERM_ERROR) {
        zmm_printf("Failed to execute build script executable :(\n");
        return_code = 1;
        goto cleanup;
    } else if (s.type == TERM_SIGNALED) {
        zmm_printf("Build script executable forcibly terminated by OS\n");
        return_code = 1;
        goto cleanup;
    }

    return_code = s.code;

cleanup:
    zmmconfig_free(&cfg);
    free(cfg_str.ptr);
    return return_code;
}
