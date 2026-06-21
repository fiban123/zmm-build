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
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "args.h"
#include "num.h"
#include "print.h"
#include "str.h"
#include "sys.h"
#include "vec.h"

typedef struct {
    vec(StringView) build_script_srcs;
    StringView build_script_exe;
    StringView build_script_compile_cmd;
} ZmmConfig;

static void zmmconfig_free(ZmmConfig* cfg) {
    for (usize i = 0; i < vecsize(cfg->build_script_srcs); ++i) {
        if (cfg->build_script_srcs[i].ptr) {
            free((void*)cfg->build_script_srcs[i].ptr);
        }
    }
    vecsize(cfg->build_script_srcs);

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
static StringView unescape_json_string(const char* src, usize len) {
    // The unescaped string will never be longer than the escaped string.
    char* dest = malloc(len);
    if (!dest) return (StringView){NULL, 0};

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

    return (StringView){.ptr = dest, .len = d};
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
i32 parse_zmm_config(ZmmConfig* cfg, StringView jstr) {
    cfg->build_script_srcs = NULL;
    cfg->build_script_exe = (StringView){NULL, 0};
    cfg->build_script_compile_cmd = (StringView){NULL, 0};

    jsmn_parser p;
    jsmn_init(&p);

    int tok_count = jsmn_parse(&p, jstr.ptr, jstr.len, NULL, 0);
    if (tok_count < 0) return 1;

    jsmntok_t* t = malloc(sizeof(jsmntok_t) * tok_count);
    if (!t) return 1;

    jsmn_init(&p);
    int r = jsmn_parse(&p, jstr.ptr, jstr.len, t, tok_count);

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
                        StringView src = unescape_json_string(
                            jstr.ptr + t[i].start,
                            (usize)(t[i].end - t[i].start));
                        vecpush(cfg->build_script_srcs, src);
                        i++;
                    } else {
                        i = jsmn_skip(t, r, i);
                    }
                }
            } else {
                i = jsmn_skip(t, r, i);
            }
        } else if (json_token_eq(jstr.ptr, &t[i], "build-script-exe")) {
            i++;
            if (i < r && t[i].type == JSMN_STRING) {
                // Unescape exe string
                cfg->build_script_exe = unescape_json_string(
                    jstr.ptr + t[i].start, (usize)(t[i].end - t[i].start));
            }
            i++;
        } else if (json_token_eq(jstr.ptr, &t[i], "build-script-compile-cmd")) {
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

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || \
    defined(__NetBSD__)
// macOS and BSDs use st_mtimespec
#define GET_MTIME_NSEC(st_ptr) ((st_ptr)->st_mtimespec.tv_nsec)
#elif defined(__linux__) || defined(__CYGWIN__) || \
    (defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200809L)
// Linux and modern POSIX.1-2008 use st_mtim
#define GET_MTIME_NSEC(st_ptr) ((st_ptr)->st_mtim.tv_nsec)
#else
// Safe fallback for older systems that only support second-level precision
#define GET_MTIME_NSEC(st_ptr) 0
#endif

// returns true if b is newer than a
static inline bool is_dirty_stat(const struct stat* a_stat,
                                 const struct stat* b_stat) {
    long a_sec = a_stat->st_mtime;
    long b_sec = b_stat->st_mtime;

    long a_nsec = GET_MTIME_NSEC(a_stat);
    long b_nsec = GET_MTIME_NSEC(b_stat);

    if (b_sec > a_sec) return true;
    if (a_sec == b_sec && b_nsec > a_nsec) return true;

    return false;
}

static bool is_dirty(StringView output, const StringView* inputs,
                     usize num_inputs) {
    if (num_inputs == 0) return true;
    char* out_path = zmm_str_vtoc(output);
    struct stat out_stat;
    int out_res = stat(out_path, &out_stat);
    free(out_path);

    if (out_res != 0) return true;

    for (usize i = 0; i < num_inputs; ++i) {
        char* in_path = zmm_str_vtoc(inputs[i]);
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

    String cfg_str = {.ptr = malloc(fsize), .len = fsize};
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

    i32 out = parse_zmm_config(&cfg, zmm_str_stov(cfg_str));

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
                          vecsize(cfg.build_script_srcs));

    dirty |=
        is_dirty(cfg.build_script_exe, (StringView[]){zmm_str_ctov(".zmm")}, 1);

    if (dirty) {
        zmm_printf("=> Compiling build script...\n");
        u64 start = get_us();
        // run the builder command
        // to avoid complex escaping, we just use the shell
        // posix: sh -c "..."
        // win: cmd /c "..."

        ArgvBuilder cmd;
        zmm_argvb_initcap(&cmd, 2048, 32);

#ifdef _WIN32
        zmm_argvb_append(&cmd, zmm_str_ctov("cmd"));
        zmm_argvb_append(&cmd, zmm_str_ctov("/c"));
#else
        zmm_argvb_append(&cmd, zmm_str_ctov("sh"));
        zmm_argvb_append(&cmd, zmm_str_ctov("-c"));
#endif

        zmm_argvb_append(&cmd, cfg.build_script_compile_cmd);

        ChildTerm s = zmm_sys_exec_print(cmd.argv, cmd.argv_len);

        zmm_argvb_free(&cmd);

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
                zmm_printf("Builder command failed (%i32) :(\n", s.code);
                return_code = 1;
                goto cleanup;
            }
        }

        u64 end = get_us();

        zmm_printf("Build script compiled in %u64ms\n", (end - start) / 1000);
    }

    ArgvBuilder cmd;
    zmm_argvb_initcap(&cmd, 2048, 32);

    zmm_argvb_pstart(&cmd);

#ifdef _WIN32
    zmm_argvb_pappend(&cmd, zmm_str_ctov(".\\"));
#else
    zmm_argvb_pappend(&cmd, zmm_str_ctov("./"));
#endif
    zmm_argvb_pappend(&cmd, cfg.build_script_exe);
    zmm_argvb_pfinish(&cmd);

    for (int i = 1; i < argc; i++) {
        zmm_argvb_append(&cmd,
                         (StringView){.ptr = argv[i], .len = strlen(argv[i])});
    }

    ChildTerm s = zmm_sys_exec_redirect(cmd.argv, cmd.argv_len);

    zmm_argvb_free(&cmd);

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
