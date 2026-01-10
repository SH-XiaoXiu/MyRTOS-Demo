/**
 * @file  shell_process.c
 * @brief 进程管理命令（run, jobs, kill, fg, bg, ls）
 */
#include "shell.h"

#if MYRTOS_SERVICE_PROCESS_ENABLE == 1

#include "MyRTOS_Process.h"
#include "MyRTOS_IO.h"
#include "MyRTOS.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if MYRTOS_SERVICE_VTS_ENABLE == 1
#include "MyRTOS_VTS.h"
#else
// VTS service stubs when disabled
static inline void VTS_ReturnToRootFocus(void) {}
static inline int VTS_SetFocus(StreamHandle_t input_stream, StreamHandle_t output_stream) {
    (void)input_stream;
    (void)output_stream;
    return 0;
}
#endif

// 管理前台会话：等待进程退出或信号
static void manage_foreground_session(pid_t pid) {
    if (pid <= 0) {
        return;
    }

    // 获取进程的stdin/stdout流
    StreamHandle_t stdin_pipe = Process_GetFdHandleByPid(pid, STDIN_FILENO);
    StreamHandle_t stdout_pipe = Process_GetFdHandleByPid(pid, STDOUT_FILENO);

    if (stdin_pipe == NULL || stdout_pipe == NULL) {
        return;
    }

    // 切换VTS焦点
    VTS_SetFocus(stdin_pipe, stdout_pipe);

    // 等待子进程退出或信号
    uint32_t received_signals = Task_WaitSignal(SIG_CHILD_EXIT | SIG_INTERRUPT | SIG_SUSPEND | SIG_BACKGROUND,
                                                MYRTOS_MAX_DELAY, SIGNAL_WAIT_ANY | SIGNAL_CLEAR_ON_EXIT);

    // 恢复Shell焦点
    VTS_ReturnToRootFocus();

    if (received_signals & SIG_INTERRUPT) {
        MyRTOS_printf("^C\n");
        Process_Kill(pid);
        Task_WaitSignal(SIG_CHILD_EXIT, MS_TO_TICKS(100), SIGNAL_WAIT_ANY | SIGNAL_CLEAR_ON_EXIT);
    } else if (received_signals & SIG_SUSPEND) {
        if (Process_Suspend(pid) == 0) {
            const char *name = Process_GetName(pid);
            MyRTOS_printf("\nSuspended [%d] %s\n", pid, name ? name : "unknown");
        }
    } else if (received_signals & SIG_BACKGROUND) {
        if (Process_SetMode(pid, PROCESS_MODE_BACKGROUND) == 0) {
            const char *name = Process_GetName(pid);
            MyRTOS_printf("\n[%d] %s &\n", pid, name ? name : "unknown");
        }
    }

    // 丢弃残留字符
    char dummy_char;
    while (Stream_Read(Stream_GetTaskStdIn(NULL), &dummy_char, 1, 0) > 0);
}

// jobs命令的visitor函数，打印进程信息
static bool print_job_visitor(const Process_t *proc, void *arg) {
    (void)arg;
    const char *state_str = (proc->state == PROCESS_STATE_RUNNING) ? "Running" :
                            (proc->state == PROCESS_STATE_SUSPENDED) ? "Suspended" : "Zombie";
    MyRTOS_printf("%-4d | %-12s | %s\n", proc->pid, state_str, proc->name);
    return true;
}

// jobs命令，列出所有进程
static int cmd_jobs(shell_handle_t shell, int argc, char *argv[]) {
    (void)shell;
    (void)argc;
    (void)argv;
    MyRTOS_printf("PID  | STATUS       | NAME\n");
    MyRTOS_printf("-----|--------------|----------------\n");
    Process_ForEach(print_job_visitor, NULL);
    return 0;
}

// ls命令的visitor函数，打印程序定义
static bool print_definition_visitor(const ProgramDefinition_t *definition, void *arg) {
    (void)arg;
    MyRTOS_printf("  %-15s - %s\n", definition->name, definition->help);
    return true;
}

// ls命令，列出所有可执行程序
static int cmd_ls(shell_handle_t shell, int argc, char *argv[]) {
    (void)shell;
    (void)argc;
    (void)argv;
    MyRTOS_printf("Available programs:\n");
    Process_ForEachProgram(print_definition_visitor, NULL);
    return 0;
}

// run命令，运行程序，支持前台/后台模式
static int cmd_run(shell_handle_t shell, int argc, char *argv[]) {
    (void)shell;
    if (argc < 2) {
        MyRTOS_printf("Usage: run <program_name> [args...] [&]\n");
        return -1;
    }

    bool is_background = (strcmp(argv[argc - 1], "&") == 0);
    int prog_argc = (is_background ? argc - 2 : argc - 1);
    char **prog_argv = &argv[1];
    const char *prog_name = argv[1];

    ProcessMode_t mode = is_background ? PROCESS_MODE_BACKGROUND : PROCESS_MODE_FOREGROUND;
    pid_t pid = Process_RunProgram(prog_name, prog_argc, prog_argv, mode);

    if (pid > 0) {
        if (is_background) {
            MyRTOS_printf("[%d] %s\n", pid, prog_name);
        } else {
            manage_foreground_session(pid);
        }
    } else {
        MyRTOS_printf("Error: Failed to start program '%s'.\n", prog_name);
    }
    return 0;
}

// kill命令，终止进程
static int cmd_kill(shell_handle_t shell, int argc, char *argv[]) {
    (void)shell;
    if (argc != 2) {
        MyRTOS_printf("Usage: kill <pid>\n");
        return -1;
    }

    int pid = atoi(argv[1]);
    if (pid <= 0) {
        MyRTOS_printf("Error: Invalid PID.\n");
        return -1;
    }

    if (Process_Kill(pid) == 0) {
        MyRTOS_printf("Kill signal sent to PID %d.\n", pid);
    } else {
        MyRTOS_printf("Error: Process with PID %d not found.\n", pid);
    }
    return 0;
}

// fg命令，将进程切换到前台
static int cmd_fg(shell_handle_t shell, int argc, char *argv[]) {
    (void)shell;
    if (argc != 2) {
        MyRTOS_printf("Usage: fg <pid>\n");
        return -1;
    }

    int pid = atoi(argv[1]);
    if (pid <= 0) {
        MyRTOS_printf("Error: Invalid PID.\n");
        return -1;
    }

    // 检查进程是否存在
    const char *name = Process_GetName(pid);
    if (name == NULL) {
        MyRTOS_printf("fg: process not found: %d\n", pid);
        return -1;
    }

    MyRTOS_printf("%s\n", name);

    // TODO: 这里需要检查并创建管道（如果进程原本是后台运行的）
    // 当前版本假设进程已有stdio流

    // 如果进程被挂起，先恢复它
    if (Process_GetState(pid) == PROCESS_STATE_SUSPENDED) {
        Process_Resume(pid);
    }

    // 切换到前台
    manage_foreground_session(pid);

    return 0;
}

// bg命令，在后台恢复挂起的进程
static int cmd_bg(shell_handle_t shell, int argc, char *argv[]) {
    (void)shell;
    if (argc != 2) {
        MyRTOS_printf("Usage: bg <pid>\n");
        return -1;
    }

    int pid = atoi(argv[1]);
    if (pid <= 0) {
        MyRTOS_printf("Error: Invalid PID.\n");
        return -1;
    }

    const char *name = Process_GetName(pid);
    if (name == NULL) {
        MyRTOS_printf("bg: process not found: %d\n", pid);
        return -1;
    }

    if (Process_GetState(pid) != PROCESS_STATE_SUSPENDED) {
        MyRTOS_printf("bg: process %d is already running.\n", pid);
        return -1;
    }

    if (Process_Resume(pid) == 0) {
        MyRTOS_printf("[%d] %s &\n", pid, name);
    } else {
        MyRTOS_printf("bg: failed to resume process %d.\n", pid);
    }

    return 0;
}

void shell_register_process_commands(shell_handle_t shell) {
    shell_register_command(shell, "jobs", "列出所有作业 (别名: progs)", cmd_jobs);
    shell_register_command(shell, "progs", "jobs 命令的别名", cmd_jobs);
    shell_register_command(shell, "run", "运行程序. 用法: run <prog> [&]", cmd_run);
    shell_register_command(shell, "kill", "终止一个作业. 用法: kill <pid>", cmd_kill);
    shell_register_command(shell, "fg", "将作业切换到前台. 用法: fg <pid>", cmd_fg);
    shell_register_command(shell, "bg", "在后台恢复挂起的作业. 用法: bg <pid>", cmd_bg);
    shell_register_command(shell, "ls", "列出所有可执行程序", cmd_ls);
}

#else

// Process 服务未启用
void shell_register_process_commands(shell_handle_t shell) {
    (void)shell;
}

#endif // MYRTOS_SERVICE_PROCESS_ENABLE
