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

#include "print.h"

void exec_result_free(ExecResult* res) {
    if (res->output.ptr != NULL) {
        free(res->output.ptr);
        res->output.ptr = NULL;
        res->output.len = 0;
    }
}

// ----------------------------------------------------------------------------
// POSIX Implementation
// ----------------------------------------------------------------------------
#ifndef _WIN32

ExecResult zmm_sys_exec(const char* arg_buf, usize num_args) {
    ExecResult res = {.status = {.term = {TERM_ERROR, -1}, .err = EXEC_SUCCESS},
                      .output = NullSliceU8};

    int pipe_fds[2];
    if (pipe(pipe_fds) != 0) {
        res.status.err = EXEC_ERR_PIPE;
        return res;
    }

    fcntl(pipe_fds[0], F_SETFD, FD_CLOEXEC);
    fcntl(pipe_fds[1], F_SETFD, FD_CLOEXEC);

    pid_t pid = fork();
    if (pid < 0) {
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        res.status.err = EXEC_ERR_FORK;
        return res;
    }

    if (pid == 0) {
        // Child
        dup2(pipe_fds[1], STDOUT_FILENO);
        dup2(pipe_fds[1], STDERR_FILENO);
        close(pipe_fds[0]);
        close(pipe_fds[1]);

        char* argv_ptrs[num_args + 1];
        const char* current = arg_buf;
        for (usize i = 0; i < num_args; i++) {
            argv_ptrs[i] = (char*)current;
            current += strlen(current) + 1;
        }
        argv_ptrs[num_args] = NULL;

        execvp(argv_ptrs[0], argv_ptrs);
        _exit(1);
    }

    // Parent
    close(pipe_fds[1]);

    usize cap = 4096;
    usize len = 0;
    u8* out_buf = (u8*)malloc(cap);

    if (!out_buf) {
        close(pipe_fds[0]);
        res.status.err = EXEC_ERR_OOM;
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
        res.status.term.type = TERM_EXITED;
        res.status.term.code = WEXITSTATUS(wait_status);
    } else if (WIFSIGNALED(wait_status)) {
        res.status.term.type = TERM_SIGNALED;
        res.status.term.code = WTERMSIG(wait_status);
    }

    res.output.ptr = out_buf;
    res.output.len = len;
    return res;
}

ExecStatus zmm_sys_exec_redirect(const char* arg_buf, usize num_args) {
    ExecStatus status = {.term = {TERM_ERROR, -1}, .err = EXEC_SUCCESS};

    pid_t pid = fork();
    if (pid < 0) {
        status.err = EXEC_ERR_FORK;
        return status;
    }

    if (pid == 0) {
        char* argv_ptrs[num_args + 1];
        const char* current = arg_buf;
        for (usize i = 0; i < num_args; i++) {
            argv_ptrs[i] = (char*)current;
            current += strlen(current) + 1;
        }
        argv_ptrs[num_args] = NULL;

        execvp(argv_ptrs[0], argv_ptrs);
        _exit(1);
    }

    int wait_status = 0;
    waitpid(pid, &wait_status, 0);

    if (WIFEXITED(wait_status)) {
        status.term.type = TERM_EXITED;
        status.term.code = WEXITSTATUS(wait_status);
    } else if (WIFSIGNALED(wait_status)) {
        status.term.type = TERM_SIGNALED;
        status.term.code = WTERMSIG(wait_status);
    }

    return status;
}

// ----------------------------------------------------------------------------
// Windows Implementation
// ----------------------------------------------------------------------------
#else

static char* build_win32_cmdline(const char* arg_buf, usize num_args) {
    usize cmd_cap = 1024;
    char* cmd = (char*)malloc(cmd_cap);
    if (!cmd) return NULL;

    usize cmd_len = 0;
    const char* current = arg_buf;

    for (usize i = 0; i < num_args; i++) {
        usize arg_len = strlen(current);
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
        cmd[cmd_len++] = '"';
        memcpy(cmd + cmd_len, current, arg_len);
        cmd_len += arg_len;
        cmd[cmd_len++] = '"';

        current += arg_len + 1;
    }
    cmd[cmd_len] = '\0';
    return cmd;
}

ExecResult exec_cmd(const char* arg_buf, usize num_args) {
    ExecResult res = {.status = {.term = {TERM_ERROR, -1}, .err = EXEC_SUCCESS},
                      .output = NullSliceU8};

    SECURITY_ATTRIBUTES sa = {.nLength = sizeof(SECURITY_ATTRIBUTES),
                              .bInheritHandle = TRUE,
                              .lpSecurityDescriptor = NULL};

    HANDLE read_pipe = NULL;
    HANDLE write_pipe = NULL;
    if (!CreatePipe(&read_pipe, &write_pipe, &sa, 0)) {
        res.status.err = EXEC_ERR_PIPE;
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
    char* cmd_line = build_win32_cmdline(arg_buf, num_args);
    if (!cmd_line) {
        CloseHandle(read_pipe);
        CloseHandle(write_pipe);
        res.status.err = EXEC_ERR_OOM;
        return res;
    }

    if (!CreateProcessA(NULL, cmd_line, NULL, NULL, TRUE, 0, NULL, NULL, &si,
                        &pi)) {
        CloseHandle(write_pipe);
        CloseHandle(read_pipe);
        free(cmd_line);
        res.status.err = EXEC_ERR_CREATE_PROCESS;
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
        res.status.err = EXEC_ERR_OOM;
        return res;
    }

    u8 buffer[4096];
    while (true) {
        DWORD bytes_read = 0;
        if (!ReadFile(read_pipe, buffer, sizeof(buffer), &bytes_read, NULL) ||
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

    res.status.term.type = TERM_EXITED;
    res.status.term.code = (i32)exit_code;
    res.output.ptr = out_buf;
    res.output.len = len;

    return res;
}

ExecStatus exec_redirect(const char* arg_buf, usize num_args) {
    ExecStatus status = {.term = {TERM_ERROR, -1}, .err = EXEC_SUCCESS};

    STARTUPINFOA si = {0};
    si.cb = sizeof(STARTUPINFOA);
    PROCESS_INFORMATION pi = {0};

    char* cmd_line = build_win32_cmdline(arg_buf, num_args);
    if (!cmd_line) {
        status.err = EXEC_ERR_OOM;
        return status;
    }

    if (!CreateProcessA(NULL, cmd_line, NULL, NULL, TRUE, 0, NULL, NULL, &si,
                        &pi)) {
        free(cmd_line);
        status.err = EXEC_ERR_CREATE_PROCESS;
        return status;
    }
    free(cmd_line);
    CloseHandle(pi.hThread);

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);

    status.term.type = TERM_EXITED;
    status.term.code = (i32)exit_code;
    return status;
}

#endif

// ----------------------------------------------------------------------------
// Shared Implementation
// ----------------------------------------------------------------------------

ExecStatus zmm_sys_exec_print(const char* arg_buf, usize num_args) {
    ExecResult res = zmm_sys_exec(arg_buf, num_args);

    if (res.status.err == EXEC_SUCCESS && res.output.len > 0) {
        SliceCU8 out_slice = {.ptr = res.output.ptr, .len = res.output.len};
        zmm_printf("%s\n", out_slice);
    }

    // Save the status to return, then free the memory
    ExecStatus final_status = res.status;
    exec_result_free(&res);

    return final_status;
}
