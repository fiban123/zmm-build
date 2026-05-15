/*
 * zmm-build: A compiled C build system
 * Copyright (C) 2026 @fiban123
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "sys.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "args.h"
#include "print.h"

// ----------------------------------------------------------------------------
// POSIX Implementation
// ----------------------------------------------------------------------------
#ifndef _WIN32

ExecResult zmm_sys_exec(char* const* argv, usize num_args) {
    zmm_argv_print(argv, num_args);
    ExecResult res = {.status = {.type = TERM_ERROR, .code = -1},
                      .output = {0}};

    int pipe_fds[2];
    if (pipe(pipe_fds) != 0) {
        return res;
    }

    fcntl(pipe_fds[0], F_SETFD, FD_CLOEXEC);
    fcntl(pipe_fds[1], F_SETFD, FD_CLOEXEC);

    pid_t pid = fork();
    if (pid < 0) {
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        return res;
    }

    if (pid == 0) {
        // Child
        dup2(pipe_fds[1], STDOUT_FILENO);
        dup2(pipe_fds[1], STDERR_FILENO);
        close(pipe_fds[0]);
        close(pipe_fds[1]);

        execvp(argv[0], argv);
        _exit(1);
    }

    // Parent
    close(pipe_fds[1]);

    usize cap = 4096;
    usize len = 0;
    u8* out_buf = (u8*)malloc(cap);

    if (!out_buf) {
        close(pipe_fds[0]);
        return res;
    }

    u8 buffer[4096];
    while (true) {
        ssize_t bytes = read(pipe_fds[0], buffer, sizeof(buffer));
        if (bytes <= 0) break;

        if (len + bytes > cap) {
            cap = (len + bytes) * 2;
            u8* new_buf = (u8*)realloc(out_buf, cap);
            if (!new_buf) break;
            out_buf = new_buf;
        }
        memcpy(out_buf + len, buffer, bytes);
        len += bytes;
    }
    close(pipe_fds[0]);

    int wait_status = 0;
    waitpid(pid, &wait_status, 0);

    if (WIFEXITED(wait_status)) {
        res.status.type = TERM_EXITED;
        res.status.code = WEXITSTATUS(wait_status);
    } else if (WIFSIGNALED(wait_status)) {
        res.status.type = TERM_SIGNALED;
        res.status.code = WTERMSIG(wait_status);
    }

    res.output.ptr = out_buf;
    res.output.len = len;
    return res;
}

ChildTerm zmm_sys_exec_redirect(char* const* argv, usize num_args) {
    zmm_argv_print(argv, num_args);
    ChildTerm status = {TERM_ERROR, -1};

    pid_t pid = fork();
    if (pid < 0) return status;

    if (pid == 0) {
        execvp(argv[0], argv);
        _exit(1);
    }

    int wait_status = 0;
    waitpid(pid, &wait_status, 0);

    if (WIFEXITED(wait_status)) {
        status.type = TERM_EXITED;
        status.code = WEXITSTATUS(wait_status);
    } else if (WIFSIGNALED(wait_status)) {
        status.type = TERM_SIGNALED;
        status.code = WTERMSIG(wait_status);
    }

    return status;
}

// ----------------------------------------------------------------------------
// Windows Implementation
// ----------------------------------------------------------------------------
#else

static char* build_win32_cmdline(char* const* argv, usize num_args) {
    usize cmd_cap = 1024;
    char* cmd = (char*)malloc(cmd_cap);
    if (!cmd) return NULL;

    usize cmd_len = 0;

    for (usize i = 0; i < num_args; i++) {
        usize arg_len = strlen(argv[i]);

        if (cmd_len + arg_len + 4 > cmd_cap) {
            cmd_cap = (cmd_len + arg_len + 4) * 2;
            char* new_cmd = (char*)realloc(cmd, cmd_cap);
            if (!new_cmd) {
                free(cmd);
                return NULL;
            }
            cmd = new_cmd;
        }

        if (i > 0) cmd[cmd_len++] = ' ';

        bool needs_quotes = (arg_len == 0);
        for (usize j = 0; j < arg_len; j++) {
            if (argv[i][j] == ' ' || argv[i][j] == '\t') {
                needs_quotes = true;
                break;
            }
        }

        if (needs_quotes) cmd[cmd_len++] = '"';
        memcpy(cmd + cmd_len, argv[i], arg_len);
        cmd_len += arg_len;
        if (needs_quotes) cmd[cmd_len++] = '"';
    }
    cmd[cmd_len] = '\0';
    return cmd;
}

ExecResult zmm_sys_exec(char* const* argv, usize num_args) {
    ExecResult res = {.status = {.type = TERM_ERROR, .code = -1},
                      .output = {0}};

    SECURITY_ATTRIBUTES sa = {.nLength = sizeof(SECURITY_ATTRIBUTES),
                              .bInheritHandle = TRUE,
                              .lpSecurityDescriptor = NULL};

    HANDLE read_pipe = NULL;
    HANDLE write_pipe = NULL;
    if (!CreatePipe(&read_pipe, &write_pipe, &sa, 0)) {
        return res;
    }

    SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {0};
    si.cb = sizeof(STARTUPINFOA);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = write_pipe;
    si.hStdError = write_pipe;

    PROCESS_INFORMATION pi = {0};
    char* cmd_line = build_win32_cmdline(argv, num_args);
    if (!cmd_line) {
        CloseHandle(read_pipe);
        CloseHandle(write_pipe);
        return res;
    }

    if (!CreateProcessA(NULL, cmd_line, NULL, NULL, TRUE, 0, NULL, NULL, &si,
                        &pi)) {
        CloseHandle(write_pipe);
        CloseHandle(read_pipe);
        free(cmd_line);
        return res;
    }
    free(cmd_line);
    CloseHandle(pi.hThread);
    CloseHandle(write_pipe);

    usize cap = 4096;
    usize len = 0;
    u8* out_buf = (u8*)malloc(cap);

    if (!out_buf) {
        CloseHandle(read_pipe);
        CloseHandle(pi.hProcess);
        return res;
    }

    u8 buffer[4096];
    while (true) {
        DWORD bytes_read = 0;
        if (!ReadFile(read_pipe, buffer, (DWORD)sizeof(buffer), &bytes_read,
                      NULL) ||
            bytes_read == 0) {
            break;
        }

        if (len + bytes_read > cap) {
            cap = (len + bytes_read) * 2;
            u8* new_buf = (u8*)realloc(out_buf, cap);
            if (!new_buf) break;
            out_buf = new_buf;
        }
        memcpy(out_buf + len, buffer, bytes_read);
        len += bytes_read;
    }
    CloseHandle(read_pipe);

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);

    res.status.type = TERM_EXITED;
    res.status.code = (i32)exit_code;
    res.output.ptr = out_buf;
    res.output.len = len;

    return res;
}

ChildTerm zmm_sys_exec_redirect(char* const* argv, usize num_args) {
    ChildTerm status = {TERM_ERROR, -1};

    STARTUPINFOA si = {0};
    si.cb = sizeof(STARTUPINFOA);
    PROCESS_INFORMATION pi = {0};

    char* cmd_line = build_win32_cmdline(argv, num_args);
    if (!cmd_line) return status;

    if (!CreateProcessA(NULL, cmd_line, NULL, NULL, TRUE, 0, NULL, NULL, &si,
                        &pi)) {
        free(cmd_line);
        return status;
    }
    free(cmd_line);
    CloseHandle(pi.hThread);

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);

    status.type = TERM_EXITED;
    status.code = (i32)exit_code;
    return status;
}

#endif

// ----------------------------------------------------------------------------
// Shared Implementation
// ----------------------------------------------------------------------------

ChildTerm zmm_sys_exec_print(char* const* argv, usize num_args) {
    ExecResult res = zmm_sys_exec(argv, num_args);

    if (res.status.type != TERM_ERROR && res.output.len > 0) {
        SliceCU8 out_slice = {.ptr = res.output.ptr, .len = res.output.len};
        zmm_printf("%s\n", out_slice);
    }

    ChildTerm final_status = res.status;
    free(res.output.ptr);

    return final_status;
}

ExecResult zmm_sys_exec_flat(const char* arg_buf, usize num_args) {
    if (num_args == 0) {
        return (ExecResult){.status = {TERM_ERROR, -1}, .output = {0}};
    }

    char** argv = (char**)malloc((num_args + 1) * sizeof(char*));
    if (!argv) {
        return (ExecResult){.status = {TERM_ERROR, -1}, .output = {0}};
    }

    const char* current = arg_buf;
    for (usize i = 0; i < num_args; i++) {
        argv[i] = (char*)current;
        current += strlen(current) + 1;
    }
    argv[num_args] = NULL;

    ExecResult res = zmm_sys_exec(argv, num_args);

    free(argv);
    return res;
}

ChildTerm zmm_sys_exec_print_flat(const char* arg_buf, usize num_args) {
    ExecResult res = zmm_sys_exec_flat(arg_buf, num_args);

    if (res.status.type != TERM_ERROR && res.output.len > 0) {
        SliceCU8 out_slice = {.ptr = res.output.ptr, .len = res.output.len};
        zmm_printf("%s\n", out_slice);
    }

    ChildTerm final_status = res.status;
    free(res.output.ptr);

    return final_status;
}

ChildTerm zmm_sys_exec_redirect_flat(const char* arg_buf, usize num_args) {
    if (num_args == 0) {
        return (ChildTerm){TERM_ERROR, -1};
    }

    char** argv = (char**)malloc((num_args + 1) * sizeof(char*));
    if (!argv) {
        return (ChildTerm){TERM_ERROR, -1};
    }

    const char* current = arg_buf;
    for (usize i = 0; i < num_args; i++) {
        argv[i] = (char*)current;
        current += strlen(current) + 1;
    }
    argv[num_args] = NULL;

    ChildTerm status = zmm_sys_exec_redirect(argv, num_args);

    free(argv);
    return status;
}
