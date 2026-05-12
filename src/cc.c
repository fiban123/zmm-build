#include "cc.h"

#include <jsmn/jsmn.h>
#include <khash/khash.h>
#include <stb/stb_ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fs.h"

// FNV-1a hash for length-based slices
static inline khint_t hash_slice(SliceCU8 s) {
    khint_t h = 2166136261u;
    for (usize i = 0; i < s.len; ++i) {
        h = (h ^ s.ptr[i]) * 16777619u;
    }
    return h;
}

static inline int cmp_slice(SliceCU8 a, SliceCU8 b) {
    if (a.len != b.len) return 0;
    return memcmp(a.ptr, b.ptr, a.len) == 0;
}

// Instantiate the map type: kh_cc_t
KHASH_INIT(cc_map, SliceCU8, usize, 1, hash_slice, cmp_slice)

// --- Fast I/O Buffer ---
typedef struct {
    FILE* f;
    char buf[8192];
    usize len;
} OutBuffer;

static inline void ob_flush(OutBuffer* ob) {
    if (ob->len) {
        fwrite(ob->buf, 1, ob->len, ob->f);
        ob->len = 0;
    }
}

static inline void ob_write(OutBuffer* ob, const void* data, usize len) {
    const char* d = (const char*)data;
    while (len > 0) {
        usize space = sizeof(ob->buf) - ob->len;
        usize chunk = len < space ? len : space;
        memcpy(ob->buf + ob->len, d, chunk);
        ob->len += chunk;
        d += chunk;
        len -= chunk;
        if (ob->len >= sizeof(ob->buf)) ob_flush(ob);
    }
}

static inline void ob_puts(OutBuffer* ob, const char* s) {
    ob_write(ob, s, strlen(s));
}

static inline void ob_putc(OutBuffer* ob, char c) {
    if (ob->len >= sizeof(ob->buf)) ob_flush(ob);
    ob->buf[ob->len++] = c;
}

// --- Internal JSON Helpers ---
static inline bool jsoneq(const char* json, jsmntok_t* tok, const char* s) {
    if (tok->type == JSMN_STRING &&
        (usize)(tok->end - tok->start) == strlen(s)) {
        return strncmp(json + tok->start, s, tok->end - tok->start) == 0;
    }
    return false;
}

static int skip_token(jsmntok_t* t, int num_tokens, int current) {
    int end = t[current].end;
    int next = current + 1;
    while (next < num_tokens && t[next].start < end) next++;
    return next;
}

// Writes an unescaped JSON string into an existing buffer. Returns actual
// length written.
static usize unescape_string_into(const char* json, jsmntok_t* tok,
                                  char* dest) {
    usize out_idx = 0;
    for (int i = tok->start; i < tok->end; i++) {
        if (json[i] == '\\' && i + 1 < tok->end) {
            i++;
            switch (json[i]) {
                case 'n':
                    dest[out_idx++] = '\n';
                    break;
                case 'r':
                    dest[out_idx++] = '\r';
                    break;
                case 't':
                    dest[out_idx++] = '\t';
                    break;
                default:
                    dest[out_idx++] = json[i];
                    break;
            }
        } else {
            dest[out_idx++] = json[i];
        }
    }
    dest[out_idx] = '\0';
    return out_idx;
}

static SliceU8 unescape_string(const char* json, jsmntok_t* tok,
                               ArenaAlloc* arena) {
    usize max_len = tok->end - tok->start;
    char* dest = zmm_arena_alloc(arena, max_len + 1);
    usize actual_len = unescape_string_into(json, tok, dest);
    return (SliceU8){.ptr = (u8*)dest, .len = actual_len};
}

// --- Public Compile Commands API ---

i32 zmm_cc_parse(CompileCommands* cc, ArenaAlloc* arena, SliceCU8 path) {
    memset(cc, 0, sizeof(CompileCommands));

    char* nul_path = slice_to_cstr(path);
    if (!nul_path) return -1;

    FILE* f = fopen(nul_path, "rb");
    free(nul_path);
    if (!f) return -1;  // File not found or unreadable

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize < 0) {
        fclose(f);
        return -1;
    }

    char* json_str = malloc(fsize + 1);
    if (!json_str) {
        fclose(f);
        return -1;  // Out of memory
    }

    fread(json_str, 1, fsize, f);
    fclose(f);
    json_str[fsize] = '\0';

    usize json_len = (usize)fsize;

    // 2. Parse the JSON
    jsmn_parser p;
    jsmn_init(&p);

    int tok_count = jsmn_parse(&p, json_str, json_len, NULL, 0);
    if (tok_count < 0) {
        free(json_str);
        return -1;
    }

    jsmntok_t* t = malloc(sizeof(jsmntok_t) * tok_count);
    jsmn_init(&p);
    jsmn_parse(&p, json_str, json_len, t, tok_count);

    if (tok_count < 1 || t[0].type != JSMN_ARRAY) {
        free(t);
        free(json_str);
        return -1;
    }

    // 3. Extract the tokens
    int i = 1;
    while (i < tok_count) {
        if (t[i].type != JSMN_OBJECT) {
            i = skip_token(t, tok_count, i);
            continue;
        }

        int obj_keys = t[i].size;
        i++;

        CompileCommand cmd = {0};

        for (int k = 0; k < obj_keys; k++) {
            jsmntok_t* key = &t[i];
            int val_idx = i + 1;

            if (jsoneq(json_str, key, "directory")) {
                // 1. Unescape into a temporary buffer to avoid Arena bloat
                usize max_len = t[val_idx].end - t[val_idx].start;
                char* temp_dir = malloc(max_len + 1);
                usize actual_len =
                    unescape_string_into(json_str, &t[val_idx], temp_dir);

                // 2. Check if it's a new directory block
                if (!cc->current_directory ||
                    strcmp(cc->current_directory, temp_dir) != 0) {
                    cc->current_directory =
                        zmm_arena_dupe(arena, temp_dir, actual_len + 1);
                }

                cmd.directory = cc->current_directory;
                free(temp_dir);  // Clean up the temp buffer

                i = skip_token(t, tok_count, val_idx);
            } else if (jsoneq(json_str, key, "file")) {
                cmd.file = slice_cast(
                    SliceCU8, unescape_string(json_str, &t[val_idx], arena));
                i = skip_token(t, tok_count, val_idx);
            } else if (jsoneq(json_str, key, "arguments")) {
                if (t[val_idx].type == JSMN_ARRAY) {
                    cmd.num_args = t[val_idx].size;

                    // 1. Calculate max buffer size needed to pack all args
                    // tightly
                    usize max_packed_len = 0;
                    int arg_i = val_idx + 1;
                    for (usize a = 0; a < cmd.num_args; a++) {
                        max_packed_len +=
                            (t[arg_i].end - t[arg_i].start) + 1;  // +1 for '\0'
                        arg_i = skip_token(t, tok_count, arg_i);
                    }

                    // 2. Allocate the exact packed buffer size (unaligned)
                    cmd.args = zmm_arena_alloc(arena, max_packed_len);
                    char* current_arg_ptr = cmd.args;

                    // 3. Unescape directly into the packed buffer
                    arg_i = val_idx + 1;
                    for (usize a = 0; a < cmd.num_args; a++) {
                        usize actual_len = unescape_string_into(
                            json_str, &t[arg_i], current_arg_ptr);
                        current_arg_ptr +=
                            actual_len +
                            1;  // Advance pointer past string + null
                        arg_i = skip_token(t, tok_count, arg_i);
                    }
                    i = arg_i;
                } else {
                    i = skip_token(t, tok_count, val_idx);
                }
            } else {
                i = skip_token(t, tok_count, val_idx);
            }
        }
        if (!cc->map) cc->map = kh_init(cc_map);
        khash_t(cc_map)* h = (khash_t(cc_map)*)cc->map;

        int ret;
        khint_t k = kh_put(cc_map, h, cmd.file, &ret);
        if (ret == 0) {
            cc->items[kh_val(h, k)] = cmd;
        } else {
            kh_val(h, k) = arrlen(cc->items);
            arrpush(cc->items, cmd);
        }
    }

    free(t);

    free(json_str);

    SliceU8 cwd = zmm_fs_abs_cwd();
    char* cwd_nul = zmm_arena_alloc(arena, cwd.len + 1);
    memcpy(cwd_nul, cwd.ptr, cwd.len);
    cwd_nul[cwd.len] = '\0';

    free(cwd.ptr);

    cc->current_directory = cwd_nul;

    return 0;
}

i32 zmm_cc_append(CompileCommands* cc, ArenaAlloc* arena, SliceCU8 file,
                  const char* args, usize num_args) {
    // 1. Prepare the new command data
    CompileCommand new_cmd;
    new_cmd.directory = cc->current_directory;
    new_cmd.num_args = num_args;
    new_cmd.file.ptr = zmm_arena_dupe(arena, file.ptr, file.len);
    new_cmd.file.len = file.len;

    usize packed_len = 0;
    const char* curr = args;
    for (usize i = 0; i < num_args; i++) {
        usize slen = strlen(curr);
        packed_len += slen + 1;
        curr += slen + 1;
    }
    new_cmd.args = zmm_arena_dupe(arena, args, packed_len);

    mtx_lock(&cc->lock);

    if (!cc->map) cc->map = kh_init(cc_map);
    khash_t(cc_map)* h = (khash_t(cc_map)*)cc->map;

    int ret;
    khint_t k = kh_put(cc_map, h, new_cmd.file, &ret);

    if (ret == 0) {
        usize existing_idx = kh_val(h, k);
        cc->items[existing_idx] = new_cmd;
    } else {
        usize new_idx = arrlen(cc->items);
        arrpush(cc->items, new_cmd);
        kh_val(h, k) = new_idx;
    }

    mtx_unlock(&cc->lock);
    return 0;
}
// Updated to match the raw-parameter signature of append
i32 zmm_cc_append_dir(CompileCommands* cc, ArenaAlloc* arena, SliceCU8 file,
                      const char* args, usize num_args, SliceCU8 dir) {
    // Check if the directory changed
    if (!cc->current_directory ||
        strncmp(cc->current_directory, (const char*)dir.ptr, dir.len) != 0) {
        // Duplicate the new directory onto the arena and make it the active one
        cc->current_directory = zmm_arena_dupe(arena, dir.ptr, dir.len + 1);
        if (!cc->current_directory) return -1;
    }

    // Proceed with normal append
    return zmm_cc_append(cc, arena, file, args, num_args);
}

static void write_escaped_string(OutBuffer* ob, const u8* ptr, usize len) {
    ob_putc(ob, '"');
    for (usize i = 0; i < len; i++) {
        switch (ptr[i]) {
            case '"':
                ob_puts(ob, "\\\"");
                break;
            case '\\':
                ob_puts(ob, "\\\\");
                break;
            case '\n':
                ob_puts(ob, "\\n");
                break;
            case '\r':
                ob_puts(ob, "\\r");
                break;
            case '\t':
                ob_puts(ob, "\\t");
                break;
            default:
                ob_putc(ob, ptr[i]);
                break;
        }
    }
    ob_putc(ob, '"');
}

i32 zmm_cc_write(CompileCommands* cc, SliceCU8 path) {
    char* path_nul = slice_to_cstr(path);
    if (!path_nul) return 1;

    FILE* f = fopen(path_nul, "wb");
    free(path_nul);
    if (!f) return -1;

    OutBuffer ob = {.f = f, .len = 0};
    ob_puts(&ob, "[\n");

    usize num_cmds = arrlen(cc->items);
    for (usize i = 0; i < num_cmds; i++) {
        CompileCommand* cmd = &cc->items[i];

        ob_puts(&ob, "    {\n        \"directory\": ");
        if (cmd->directory) {
            write_escaped_string(&ob, (const u8*)cmd->directory,
                                 strlen(cmd->directory));
        } else {
            ob_puts(&ob, "\"\"");  // Fallback if somehow NULL
        }
        ob_puts(&ob, ",\n        \"file\": ");
        write_escaped_string(&ob, cmd->file.ptr, cmd->file.len);
        ob_puts(&ob, ",\n        \"arguments\": [\n");

        // Walk through the tightly packed null-terminated arguments
        char* curr_arg = cmd->args;
        for (usize a = 0; a < cmd->num_args; a++) {
            ob_puts(&ob, "            ");
            write_escaped_string(&ob, (const u8*)curr_arg, strlen(curr_arg));
            if (a < cmd->num_args - 1) ob_puts(&ob, ",");
            ob_puts(&ob, "\n");

            curr_arg += strlen(curr_arg) + 1;  // Advance to next argument
        }

        ob_puts(&ob, "        ]\n    }");
        if (i < num_cmds - 1) ob_puts(&ob, ",");
        ob_puts(&ob, "\n");
    }

    ob_puts(&ob, "]\n");
    ob_flush(&ob);  // Flush anything left in the buffer!
    fclose(f);
    return 0;
}

void zmm_cc_free(CompileCommands* cc) {
    if (cc->map) {
        kh_destroy(cc_map, (khash_t(cc_map)*)cc->map);
    }
    arrfree(cc->items);
    memset(cc, 0, sizeof(CompileCommands));
}
