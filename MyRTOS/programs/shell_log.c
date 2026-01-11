/**
 * @file  shell_log.c
 * @brief 日志命令（logall, log, loglevel）
 */
#include "include/shell.h"

#if MYRTOS_SERVICE_LOG_ENABLE == 1

#include "MyRTOS_Log.h"
#include "MyRTOS_IO.h"
#include "MyRTOS.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if MYRTOS_SERVICE_PROCESS_ENABLE == 1
#include "MyRTOS_Process.h"
#endif

#if MYRTOS_SERVICE_VTS_ENABLE == 1
#include "MyRTOS_VTS.h"
#else
// VTS service stubs when disabled
static inline StreamHandle_t VTS_GetBackgroundStream(void) { return NULL; }
static inline void VTS_ReturnToRootFocus(void) {}
static inline int VTS_SetFocus(StreamHandle_t input_stream, StreamHandle_t output_stream) {
    (void)input_stream;
    (void)output_stream;
    return 0;
}
static inline void VTS_SetTerminalMode(int mode) { (void)mode; }
#define VTS_MODE_CANONICAL 0
#define VTS_MODE_RAW 1
#endif

// 用于 logall 命令的全局日志监听器句柄
static LogListenerHandle_t g_logall_listener_h = NULL;

// logall 命令：切换全局日志监控
static int cmd_logall(shell_handle_t shell, int argc, char *argv[]) {
    (void)shell;
    if (argc != 2) {
        MyRTOS_printf("Usage: logall <on|off>\n");
        return -1;
    }
    if (strcmp(argv[1], "on") == 0) {
        if (g_logall_listener_h != NULL) {
            MyRTOS_printf("Global log monitoring is already enabled.\n");
            return 0;
        }
        // 添加一个监听所有tag(*)的监听器到VTS后台流
        g_logall_listener_h = Log_AddListener(VTS_GetBackgroundStream(), LOG_LEVEL_DEBUG, NULL);
        if (g_logall_listener_h) {
            MyRTOS_printf("Global log monitoring enabled.\n");
        } else {
            MyRTOS_printf("Error: Failed to enable global log monitoring.\n");
        }
    } else if (strcmp(argv[1], "off") == 0) {
        if (g_logall_listener_h == NULL) {
            MyRTOS_printf("Global log monitoring is not active.\n");
            return 0;
        }
        // 移除监听器
        if (Log_RemoveListener(g_logall_listener_h) == 0) {
            g_logall_listener_h = NULL;
            MyRTOS_printf("Global log monitoring disabled.\n");
        } else {
            MyRTOS_printf("Error: Failed to disable global log monitoring.\n");
        }
    } else {
        MyRTOS_printf("Invalid argument. Use 'on' or 'off'.\n");
        return -1;
    }
    return 0;
}

// log 命令：监控指定标签或任务的实时日志（Shell built-in版本，启动独立进程）
static int cmd_log(shell_handle_t shell, int argc, char *argv[]) {
    (void)shell;

#if MYRTOS_SERVICE_PROCESS_ENABLE == 1
    // 使用进程模式执行 log 程序
    // 将参数传递给进程：log <tag> [level]
    if (argc < 2) {
        MyRTOS_printf("Usage: log <tag> [level]\n");
        MyRTOS_printf("Levels: error, warn, info, debug (default: debug)\n");
        return -1;
    }

    // 构造程序名称和参数
    char *prog_argv[4];
    prog_argv[0] = "log";
    for (int i = 1; i < argc && i < 4; i++) {
        prog_argv[i] = argv[i];
    }

    // 启动 log 进程（前台模式，BOUND 绑定到当前进程）
    pid_t pid = Process_RunProgram("log", argc, prog_argv, PROCESS_MODE_BOUND);

    if (pid <= 0) {
        MyRTOS_printf("Error: Failed to start log program.\n");
        return -1;
    }

    // 获取 shell 和 log 进程的 I/O 流
    pid_t shell_pid = getpid();
    StreamHandle_t shell_stdin = Process_GetFdHandleByPid(shell_pid, STDIN_FILENO);
    StreamHandle_t shell_stdout = Process_GetFdHandleByPid(shell_pid, STDOUT_FILENO);
    StreamHandle_t stdin_pipe = Process_GetFdHandleByPid(pid, STDIN_FILENO);
    StreamHandle_t stdout_pipe = Process_GetFdHandleByPid(pid, STDOUT_FILENO);

    // 切换 VTS 焦点到 log 进程
    if (stdin_pipe && stdout_pipe) {
        VTS_SetFocus(stdin_pipe, stdout_pipe);
    }

    // 等待子进程退出信号
    Task_WaitSignal(SIG_CHILD_EXIT, MYRTOS_MAX_DELAY, SIGNAL_WAIT_ANY | SIGNAL_CLEAR_ON_EXIT);

    // 恢复 Shell 焦点
    VTS_SetFocus(shell_stdin, shell_stdout);

    // 获取退出码
    int exit_code = 0;
    Process_GetExitCode(pid, &exit_code);

    return exit_code;
#else
    (void)argc;
    (void)argv;
    MyRTOS_printf("Process service is not enabled.\n");
    return -1;
#endif
}

// loglevel 命令：设置或显示全局日志级别
static int cmd_loglevel(shell_handle_t shell, int argc, char *argv[]) {
    (void)shell;
    if (argc != 2) {
        MyRTOS_printf("Usage: loglevel <level>\n");
        MyRTOS_printf("Levels: 0=NONE, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG\n");
        MyRTOS_printf("Current level: %d\n", Log_GetGlobalLevel());
        return -1;
    }

    int level = atoi(argv[1]);
    if (level >= LOG_LEVEL_NONE && level <= LOG_LEVEL_DEBUG) {
        Log_SetGlobalLevel((LogLevel_t)level);
        MyRTOS_printf("Global log level set to %d.\n", level);
    } else {
        MyRTOS_printf("Invalid level. Please use a number between 0 and 4.\n");
        return -1;
    }
    return 0;
}

void shell_register_log_commands(shell_handle_t shell) {
    shell_register_command(shell, "logall", "切换全局日志. 用法: logall <on|off>", cmd_logall);
    shell_register_command(shell, "log", "监听指定Tag日志. 用法: log <tag>", cmd_log);
    shell_register_command(shell, "loglevel", "设置日志级别. 用法: loglevel <0-4>", cmd_loglevel);
}

#else

// Log 服务未启用
void shell_register_log_commands(shell_handle_t shell) {
    (void)shell;
}

#endif // MYRTOS_SERVICE_LOG_ENABLE
