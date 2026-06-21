/*
 * zmm-build: A compiled C build system
 * Copyright 2026 fiban123
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cc.h"

#include <jsmn/jsmn.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "vec.h"

// --- Buffered Output ---

static inline void ob_flush(CompileCommands* cc) {
    usize len = vecsize(cc->buf);
    if (len > 0) {
        fwrite(cc->buf, 1, len, cc->f);
        vecsetsize(cc->buf, 0);  // Reset length, but retain capacity memory
    }
}

static inline void ob_write(CompileCommands* cc, const void* data, usize len) {
    const char* d = (const char*)data;
    usize old_len = vecsize(cc->buf);
    vecsetsize(cc->buf, old_len + len);
    memcpy(cc->buf + old_len, d, len);
}

static inline void ob_puts(CompileCommands* cc, const char* s) {
    ob_write(cc, s, strlen(s));
}

static inline void ob_putc(CompileCommands* cc, char c) { vecpush(cc->buf, c); }

static void write_escaped_string(CompileCommands* cc, const char* ptr,
                                 usize len) {
    ob_putc(cc, '"');
    for (usize i = 0; i < len; i++) {
        switch (ptr[i]) {
            case '"':
                ob_puts(cc, "\\\"");
                break;
            case '\\':
                ob_puts(cc, "\\\\");
                break;
            case '\n':
                ob_puts(cc, "\\n");
                break;
            case '\r':
                ob_puts(cc, "\\r");
                break;
            case '\t':
                ob_puts(cc, "\\t");
                break;
            default:
                ob_putc(cc, ptr[i]);
                break;
        }
    }
    ob_putc(cc, '"');
}

// --- Writer API ---

API int zmm_cc_init(CompileCommands* cc, StringView path) {
    memset(cc, 0, sizeof(CompileCommands));

    char* nul_path = zmm_str_vtoc(path);
    if (!nul_path) return 1;

    cc->f = fopen(nul_path, "wb");
    free(nul_path);
    if (!cc->f) return 1;

    cc->lock = malloc(sizeof(pthread_mutex_t));
    if (!cc->lock) {
        fclose(cc->f);
        return 1;
    }
    pthread_mutex_init((pthread_mutex_t*)cc->lock, NULL);

    ob_puts(cc, "[\n");
    ob_flush(cc);

    return 0;
}

API void zmm_cc_append(CompileCommands* cc, StringView dir, StringView file,
                       const char* args, usize num_args) {
    pthread_mutex_lock((pthread_mutex_t*)cc->lock);

    if (cc->count > 0) {
        ob_puts(cc, ",\n");
    }

    ob_puts(cc, "    {\n        \"directory\": ");
    write_escaped_string(cc, dir.ptr, dir.len);
    ob_puts(cc, ",\n        \"file\": ");
    write_escaped_string(cc, file.ptr, file.len);
    ob_puts(cc, ",\n        \"arguments\": [\n");

    const char* curr_arg = args;
    for (usize a = 0; a < num_args; a++) {
        usize slen = strlen(curr_arg);
        ob_puts(cc, "            ");
        write_escaped_string(cc, curr_arg, slen);
        if (a < num_args - 1) ob_putc(cc, ',');
        ob_putc(cc, '\n');
        curr_arg += slen + 1;
    }

    ob_puts(cc, "        ]\n    }");

    cc->count++;
    ob_flush(cc);

    pthread_mutex_unlock((pthread_mutex_t*)cc->lock);
}

API void zmm_cc_finish(CompileCommands* cc) {
    ob_puts(cc, "\n]\n");
    ob_flush(cc);
    fclose(cc->f);
    cc->f = NULL;

    vecfree(cc->buf);

    if (cc->lock) {
        pthread_mutex_destroy((pthread_mutex_t*)cc->lock);
        free(cc->lock);
        cc->lock = NULL;
    }
}
// --- Parser Helpers ---

static inline bool jsoneq(const char* json, jsmntok_t* tok, const char* s) {
    if (tok->type == JSMN_STRING &&
        (usize)(tok->end - tok->start) == strlen(s)) {
        return strncmp(json + tok->start, s, tok->end - tok->start) == 0;
    }
    return false;
}

static inline int skip_token(jsmntok_t* t, int num_tokens, int current) {
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

static StringView unescape_string(const char* json, jsmntok_t* tok) {
    usize max_len = tok->end - tok->start;
    char* dest = malloc(max_len + 1);
    usize actual_len = unescape_string_into(json, tok, dest);
    return (StringView){dest, actual_len};
}

// --- Parser API ---

API int zmm_cc_parse(CompileCommand** out, StringView path) {
    char* nul_path = zmm_str_vtoc(path);
    if (!nul_path) return 1;

    FILE* f = fopen(nul_path, "r");
    free(nul_path);
    if (!f) return 1;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize < 0) {
        fclose(f);
        return 1;
    }

    char* json_str = malloc(fsize + 1);
    if (!json_str) {
        fclose(f);
        return 1;
    }

    fread(json_str, 1, fsize, f);
    fclose(f);
    json_str[fsize] = '\0';

    usize json_len = (usize)fsize;

    jsmn_parser p;
    jsmn_init(&p);

    int tok_count = jsmn_parse(&p, json_str, json_len, NULL, 0);
    if (tok_count < 0) {
        free(json_str);
        return 1;
    }

    jsmntok_t* t = malloc(sizeof(jsmntok_t) * tok_count);
    jsmn_init(&p);
    jsmn_parse(&p, json_str, json_len, t, tok_count);

    if (tok_count < 1 || t[0].type != JSMN_ARRAY) {
        free(t);
        free(json_str);
        return 1;
    }

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
                cmd.directory = unescape_string(json_str, &t[val_idx]);
                i = skip_token(t, tok_count, val_idx);
            } else if (jsoneq(json_str, key, "file")) {
                cmd.file = unescape_string(json_str, &t[val_idx]);
                i = skip_token(t, tok_count, val_idx);
            } else if (jsoneq(json_str, key, "arguments")) {
                if (t[val_idx].type == JSMN_ARRAY) {
                    cmd.num_args = t[val_idx].size;

                    usize max_packed_len = 0;
                    int arg_i = val_idx + 1;
                    for (usize a = 0; a < cmd.num_args; a++) {
                        max_packed_len += (t[arg_i].end - t[arg_i].start) + 1;
                        arg_i = skip_token(t, tok_count, arg_i);
                    }

                    cmd.args = malloc(max_packed_len);
                    char* current_arg_ptr = cmd.args;

                    arg_i = val_idx + 1;
                    for (usize a = 0; a < cmd.num_args; a++) {
                        usize actual_len = unescape_string_into(
                            json_str, &t[arg_i], current_arg_ptr);
                        current_arg_ptr += actual_len + 1;
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
        vecpush(*out, cmd);
    }

    free(t);
    free(json_str);

    return 0;
}
