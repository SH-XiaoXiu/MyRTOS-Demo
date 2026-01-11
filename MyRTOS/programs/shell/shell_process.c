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

    // 保存shell自己的stdin/stdout，以便稍后恢复焦点
    pid_t shell_pid = getpid();
    StreamHandle_t shell_stdin = Process_GetFdHandleByPid(shell_pid, STDIN_FILENO);
    StreamHandle_t shell_stdout = Process_GetFdHandleByPid(shell_pid, STDOUT_FILENO);

    // 获取进程的stdin/stdout流
    StreamHandle_t stdin_pipe = Process_GetFdHandleByPid(pid, STDIN_FILENO);
    StreamHandle_t stdout_pipe = Process_GetFdHandleByPid(pid, STDOUT_FILENO);

    if (stdin_pipe == NULL || stdout_pipe == NULL) {
        return;
    }

    // 切换VTS焦点到前台进程
    VTS_SetFocus(stdin_pipe, stdout_pipe);

    // 等待子进程退出或信号
    // shell的Task等待信号，信号接收器保持为shell自己
    uint32_t received_signals = Task_WaitSignal(SIG_CHILD_EXIT | SIG_INTERRUPT | SIG_SUSPEND | SIG_BACKGROUND,
                                                MYRTOS_MAX_DELAY, SIGNAL_WAIT_ANY | SIGNAL_CLEAR_ON_EXIT);

    // 恢复Shell焦点（恢复到shell自己的stdio，而不是root）
    VTS_SetFocus(shell_stdin, shell_stdout);

    if (received_signals & SIG_INTERRUPT) {
        Process_Kill(pid);
        Task_WaitSignal(SIG_CHILD_EXIT, MS_TO_TICKS(100), SIGNAL_WAIT_ANY | SIGNAL_CLEAR_ON_EXIT);
    } else if (received_signals & SIG_SUSPEND) {
        if (Process_Suspend(pid) == 0) {
            const char *name = Process_GetName(pid);
            MyRTOS_printf("\nSuspended [%d] %s\n", pid, name ? name : "unknown");
        }
    } else if (received_signals & SIG_BACKGROUND) {
        // Ctrl+B：shell 放弃前台等待，进程继续在后台运行
        const char *name = Process_GetName(pid);
        MyRTOS_printf("\n[%d] %s &\n", pid, name ? name : "unknown");
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
    MyRTOS_printf("%-4d | %-5d | %-12s | %s\n", proc->pid, proc->parent_pid, state_str, proc->name);
    return true;
}

// jobs命令，列出所有进程
static int cmd_jobs(shell_handle_t shell, int argc, char *argv[]) {
    (void)shell;
    (void)argc;
    (void)argv;
    MyRTOS_printf("PID  | PPID  | STATUS       | NAME\n");
    MyRTOS_printf("-----|-------|--------------|----------------\n");
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

// run命令，运行程序，支持前台/后台执行和守护进程
static int cmd_run(shell_handle_t shell, int argc, char *argv[]) {
    (void)shell;
    if (argc < 2) {
        MyRTOS_printf("Usage: run [--detach] <program_name> [args...] [&]\n");
        MyRTOS_printf("  --detach: 创建守护进程（独立于shell生命周期）\n");
        MyRTOS_printf("  &:        后台运行（不占用终端焦点）\n");
        return -1;
    }

    // 检查 --detach 参数
    int arg_idx = 1;
    ProcessMode_t mode = PROCESS_MODE_BOUND;
    if (strcmp(argv[arg_idx], "--detach") == 0) {
        mode = PROCESS_MODE_DETACHED;
        arg_idx++;
        if (arg_idx >= argc) {
            MyRTOS_printf("Error: Missing program name after --detach.\n");
            return -1;
        }
    }

    // 检查是否后台运行（&参数只影响shell行为，不影响进程生命周期）
    bool run_in_background = (strcmp(argv[argc - 1], "&") == 0);
    int prog_argc = (run_in_background ? argc - arg_idx - 1 : argc - arg_idx);
    char **prog_argv = &argv[arg_idx];
    const char *prog_name = argv[arg_idx];

    pid_t pid = Process_RunProgram(prog_name, prog_argc, prog_argv, mode);

    if (pid > 0) {
        if (run_in_background) {
            // 后台：不切换焦点，不等待
            if (mode == PROCESS_MODE_DETACHED) {
                MyRTOS_printf("[%d] %s (daemon) &\n", pid, prog_name);
            } else {
                MyRTOS_printf("[%d] %s &\n", pid, prog_name);
            }
        } else {
            // 前台：切换焦点，等待进程结束
            manage_foreground_session(pid);
        }
    } else {
        MyRTOS_printf("Error: Failed to start program '%s'.\n", prog_name);
    }
    return 0;
}

// kill命令，终止或控制进程
static int cmd_kill(shell_handle_t shell, int argc, char *argv[]) {
    (void)shell;

    // 支持：kill <pid>, kill -9 <pid>, kill -STOP <pid>, kill -CONT <pid>
    if (argc < 2) {
        MyRTOS_printf("Usage: kill [-SIGNAL] <pid>\n");
        MyRTOS_printf("Signals:\n");
        MyRTOS_printf("  (none) or -9    - Terminate process (default)\n");
        MyRTOS_printf("  -STOP or -SIGSTOP - Suspend process\n");
        MyRTOS_printf("  -CONT or -SIGCONT - Resume process\n");
        return -1;
    }

    int pid;
    const char *signal = NULL;

    // 解析参数
    if (argc == 2) {
        // kill <pid>
        pid = atoi(argv[1]);
        signal = "TERM";  // 默认信号
    } else if (argc == 3) {
        // kill -SIGNAL <pid>
        signal = argv[1];
        if (signal[0] != '-') {
            MyRTOS_printf("Error: Invalid signal format. Use -SIGNAL.\n");
            return -1;
        }
        signal++;  // 跳过 '-'
        pid = atoi(argv[2]);
    } else {
        MyRTOS_printf("Error: Too many arguments.\n");
        return -1;
    }

    if (pid <= 0) {
        MyRTOS_printf("Error: Invalid PID.\n");
        return -1;
    }

    // 执行对应的操作
    if (strcmp(signal, "9") == 0 || strcmp(signal, "TERM") == 0 ||
        strcmp(signal, "SIGTERM") == 0 || strcmp(signal, "KILL") == 0 ||
        strcmp(signal, "SIGKILL") == 0) {
        // 终止进程
        if (Process_Kill(pid) == 0) {
            MyRTOS_printf("Process %d terminated.\n", pid);
        } else {
            MyRTOS_printf("Error: Failed to kill process %d.\n", pid);
            return -1;
        }
    } else if (strcmp(signal, "STOP") == 0 || strcmp(signal, "SIGSTOP") == 0) {
        // 挂起进程
        if (Process_Suspend(pid) == 0) {
            MyRTOS_printf("Process %d suspended.\n", pid);
        } else {
            MyRTOS_printf("Error: Failed to suspend process %d.\n", pid);
            return -1;
        }
    } else if (strcmp(signal, "CONT") == 0 || strcmp(signal, "SIGCONT") == 0) {
        // 恢复进程
        if (Process_Resume(pid) == 0) {
            MyRTOS_printf("Process %d resumed.\n", pid);
        } else {
            MyRTOS_printf("Error: Failed to resume process %d.\n", pid);
            return -1;
        }
    } else {
        MyRTOS_printf("Error: Unknown signal '%s'.\n", signal);
        MyRTOS_printf("Supported signals: 9, TERM, STOP, CONT\n");
        return -1;
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
