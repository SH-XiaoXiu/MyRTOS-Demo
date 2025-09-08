#include "platform.h"

#if (MYRTOS_SERVICE_SHELL_ENABLE == 1)

#include "platform_shell_commands.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include "MyRTOS.h"
#include "MyRTOS_IO.h"
#include "MyRTOS_Monitor.h"
#include "MyRTOS_Port.h"
#include "MyRTOS_Shell_Private.h"
#include "MyRTOS_Log.h"

// VTS and Program Manager dependencies
#if MYRTOS_SERVICE_VTS_ENABLE == 1
#include "MyRTOS_VTS.h"
#else
// VTS service stubs when disabled.
static inline void VTS_ReturnToRootFocus(void) {
}
static inline int VTS_SetFocus(StreamHandle_t input_stream, StreamHandle_t output_stream) {
    (void) input_stream;
    (void) output_stream;
    return 0;
}
static inline void VTS_SetLogAllMode(bool enable) { (void) enable; }
static inline void VTS_SetTerminalMode(int mode) { (void) mode; }
#endif

#if PLATFORM_USE_PROGRAM_MANGE == 1
#include "platform_program_manager.h"
#endif

//
// 模块内部定义
//

#if PLATFORM_USE_DEFAULT_COMMANDS == 1
// Shell 命令定义结构体.
struct ShellCommandDef {
    const char *name;
    const char *help;
    ShellCommandCallback_t callback;
};

// 'ps' 命令使用的任务状态字符串表.
static const char *const g_task_state_str[] = {"Unused", "Ready", "Delayed", "Blocked", "Suspended"};
#endif

#if (MYRTOS_SERVICE_LOG_ENABLE == 1)
// 用于 logall 命令的全局日志监听器句柄.
static LogListenerHandle_t g_logall_listener_h = NULL;
#endif


//
// 私有函数声明
//

#if PLATFORM_USE_DEFAULT_COMMANDS == 1
static int cmd_help(ShellHandle_t shell_h, int argc, char *argv[]);

static int cmd_reboot(ShellHandle_t shell_h, int argc, char *argv[]);

static int cmd_ps(ShellHandle_t shell_h, int argc, char *argv[]);

static int cmd_logall(ShellHandle_t shell_h, int argc, char *argv[]);

static int cmd_log(ShellHandle_t shell_h, int argc, char *argv[]);

static int cmd_loglevel(ShellHandle_t shell_h, int argc, char *argv[]);
#if PLATFORM_USE_PROGRAM_MANGE == 1
static void manage_foreground_session(ProgramInstance_t *instance);

static bool print_job_visitor(const ProgramInstance_t *instance, void *arg);

static int cmd_jobs(ShellHandle_t shell_h, int argc, char *argv[]);

static int cmd_run(ShellHandle_t shell_h, int argc, char *argv[]);

static int cmd_kill(ShellHandle_t shell_h, int argc, char *argv[]);

static int cmd_fg(ShellHandle_t shell_h, int argc, char *argv[]);

static int cmd_bg(ShellHandle_t shell_h, int argc, char *argv[]);
#endif // PLATFORM_USE_PROGRAM_MANGE
#endif // PLATFORM_USE_DEFAULT_COMMANDS

//
// 模块全局变量
//

#if PLATFORM_USE_DEFAULT_COMMANDS == 1
// 平台默认命令表.
static const struct ShellCommandDef g_platform_commands[] = {
    {"help", "显示所有可用命令", cmd_help},
    {"reboot", "重启系统", cmd_reboot},
    {"ps", "显示系统状态", cmd_ps},
    {"logall", "切换全局日志. 用法: logall <on|off>", cmd_logall},
    {"log", "监听指定Tag日志. 用法: log <tag>", cmd_log},
    {"loglevel", "设置日志级别. 用法: loglevel <0-4>", cmd_loglevel},
#if PLATFORM_USE_PROGRAM_MANGE == 1
    {"jobs", "列出所有作业 (别名: progs)", cmd_jobs},
    {"progs", "jobs 命令的别名", cmd_jobs},
    {"run", "运行程序. 用法: run <prog> [&]", cmd_run},
    {"kill", "终止一个作业. 用法: kill <pid>", cmd_kill},
    {"fg", "将作业切换到前台. 用法: fg <pid>", cmd_fg},
    {"bg", "在后台恢复一个挂起的作业. 用法: bg <pid>", cmd_bg},
#endif
    {NULL, NULL, NULL}
};
#endif

//
// 公共 API 实现
//

#if PLATFORM_USE_DEFAULT_COMMANDS == 1
// 注册所有平台定义的默认 Shell 命令.
void Platform_RegisterDefaultCommands(ShellHandle_t shell_h) {
    for (int i = 0; g_platform_commands[i].name != NULL; ++i) {
        Shell_RegisterCommand(shell_h, g_platform_commands[i].name, g_platform_commands[i].help,
                              g_platform_commands[i].callback);
    }
}
#endif

//
// 私有函数实现
//

#if PLATFORM_USE_DEFAULT_COMMANDS == 1

// "help" 命令: 显示所有可用命令.
static int cmd_help(ShellHandle_t shell_h, int argc, char *argv[]) {
    (void) argc;
    (void) argv;
    ShellInstance_t *shell = (ShellInstance_t *) shell_h;
    MyRTOS_printf("Available Commands:\n");
    ShellCommandNode_t *node = shell->commands_head;
    while (node != NULL) {
        MyRTOS_printf("  %-12s - %s\n", node->name, node->help);
        node = node->next;
    }
    return 0;
}

// "reboot" 命令: 重启系统.
static int cmd_reboot(ShellHandle_t shell_h, int argc, char *argv[]) {
    (void) shell_h;
    (void) argc;
    (void) argv;
    MyRTOS_printf("System rebooting...\n");
    Task_Delay(MS_TO_TICKS(100));
    NVIC_SystemReset();
    return 0;
}

// "ps" 命令: 显示系统任务和内存状态.
static int cmd_ps(ShellHandle_t shell_h, int argc, char *argv[]) {
#if (MYRTOS_SERVICE_MONITOR_ENABLE == 1)
    (void) shell_h;
    (void) argc;
    (void) argv;
    const int COL_WIDTH_NAME = 16;
    TaskStats_t stats[MYRTOS_MAX_CONCURRENT_TASKS];
    int count = 0;

    MyRTOS_Port_EnterCritical();
    TaskHandle_t task = Monitor_GetNextTask(NULL);
    while (task && count < MYRTOS_MAX_CONCURRENT_TASKS) {
        Monitor_GetTaskInfo(task, &stats[count++]);
        task = Monitor_GetNextTask(task);
    }
    MyRTOS_Port_ExitCritical();

    MyRTOS_printf("\nMyRTOS Monitor (Uptime: %llu ms)\n", TICK_TO_MS(MyRTOS_GetTick()));
    MyRTOS_printf("%-*s %-4s %-8s %-10s %-18s %s\n", COL_WIDTH_NAME, "Task Name", "ID", "State", "Prio(B/C)",
                  "Stack (Used/Size)", "Runtime(us)");
    MyRTOS_printf("--\n");

    for (int i = 0; i < count; ++i) {
        TaskStats_t *s = &stats[i];
        char prio_str[12], stack_str[20];
        snprintf(prio_str, sizeof(prio_str), "%u/%u", s->base_priority, s->current_priority);
        snprintf(stack_str, sizeof(stack_str), "%u/%u", (unsigned) s->stack_high_water_mark_bytes,
                 (unsigned) s->stack_size_bytes);
        MyRTOS_printf("%-*s %-4u %-8s %-10s %-18s %llu\n", COL_WIDTH_NAME, s->task_name,
                      (unsigned)Task_GetId(s->task_handle), g_task_state_str[s->state], prio_str, stack_str,
                      s->total_runtime);
    }

    HeapStats_t heap;
    Monitor_GetHeapStats(&heap);
    MyRTOS_printf("Heap Info -> Total: %-5u | Free: %-5u | Min Ever Free: %-5u\n", (unsigned)heap.total_heap_size,
                  (unsigned)heap.free_bytes_remaining, (unsigned)heap.minimum_ever_free_bytes);
    return 0;
#else
    (void) shell_h;
    (void) argc;
    (void) argv;
    MyRTOS_printf("Monitor service is disabled.\n");
    return -1;
#endif
}

// "logall" 命令: 切换全局日志监控.
static int cmd_logall(ShellHandle_t shell_h, int argc, char *argv[]) {
    (void) shell_h;
    if (argc != 2) {
        MyRTOS_printf("Usage: logall <on|off>\n");
        return -1;
    }
    if (strcmp(argv[1], "on") == 0) {
        if (g_logall_listener_h != NULL) {
            MyRTOS_printf("Global log monitoring is already enabled.\n");
            return 0;
        }
        // 添加一个监听所有tag(*)的监听器到VTS后台流.
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
        // 移除监听器.
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

// "log" 命令: 监控指定标签或任务的实时日志
static int cmd_log(ShellHandle_t shell_h, int argc, char *argv[]) {
    (void) shell_h;
    if (argc < 2 || argc > 3) {
        MyRTOS_printf("Usage: log <taskName_or_tag> [level]\n");
        return -1;
    }

    const char *tag = argv[1];
    LogLevel_t level = LOG_LEVEL_DEBUG; // 默认级别

    if (argc == 3) {
        if (strcasecmp(argv[2], "error") == 0) {
            level = LOG_LEVEL_ERROR;
        } else if (strcasecmp(argv[2], "warn") == 0) {
            level = LOG_LEVEL_WARN;
        } else if (strcasecmp(argv[2], "info") == 0) {
            level = LOG_LEVEL_INFO;
        } else if (strcasecmp(argv[2], "debug") == 0) {
            level = LOG_LEVEL_DEBUG;
        } else if (strcasecmp(argv[2], "none") == 0) {
            level = LOG_LEVEL_NONE;
        } else {
            MyRTOS_printf("Error: Invalid log level '%s'. Use error|warn|info|debug|none.\n", argv[2]);
            return -1;
        }
    }

    StreamHandle_t pipe = Pipe_Create(1024);
    if (!pipe) {
        MyRTOS_printf("Error: Failed to create pipe for logging.\n");
        return -1;
    }

    LogListenerHandle_t listener_h = Log_AddListener(pipe, level, tag);
    if (!listener_h) {
        MyRTOS_printf("Error: Failed to add log listener.\n");
        Pipe_Delete(pipe);
        return -1;
    }

    VTS_SetFocus(Stream_GetTaskStdIn(NULL), pipe);
    VTS_SetTerminalMode(VTS_MODE_RAW);
    Stream_Printf(pipe, "--- Start monitoring LOGs with tag '%s' at level %d. Press any key to stop. ---\n",
                  tag, level);

    char ch;
    Stream_Read(Stream_GetTaskStdIn(NULL), &ch, 1, MYRTOS_MAX_DELAY);
    Stream_Printf(pipe, "\n--- Stopped monitoring tag '%s'. ---\n", tag);
    Task_Delay(MS_TO_TICKS(10));

    Log_RemoveListener(listener_h);
    VTS_ReturnToRootFocus();
    Pipe_Delete(pipe);

    return 0;
}

// "loglevel" 命令: 设置或显示全局日志级别.
static int cmd_loglevel(ShellHandle_t shell_h, int argc, char *argv[]) {
    (void) shell_h;
    if (argc != 2) {
        MyRTOS_printf("Usage: loglevel <level>\n");
        MyRTOS_printf("Levels: 0=NONE, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG\n");
        MyRTOS_printf("Current level: %d\n", Log_GetGlobalLevel());
        return -1;
    }

    int level = atoi(argv[1]);
    if (level >= LOG_LEVEL_NONE && level <= LOG_LEVEL_DEBUG) {
        Log_SetGlobalLevel((LogLevel_t) level);
        MyRTOS_printf("Global log level set to %d.\n", level);
    } else {
        MyRTOS_printf("Invalid level. Please use a number between 0 and 4.\n");
        return -1;
    }
    return 0;
}

#if PLATFORM_USE_PROGRAM_MANGE == 1

// 管理一个前台会话, 包括 VTS 焦点切换和信号处理.
static void manage_foreground_session(ProgramInstance_t *instance) {
    if (instance == NULL) {
        return;
    }

    // 将虚拟终端焦点切换到新程序.
    VTS_SetFocus(instance->stdin_pipe, instance->stdout_pipe);

    // 等待子程序退出, 或用户中断/挂起信号.
    uint32_t received_signals = Task_WaitSignal(SIG_CHILD_EXIT | SIG_INTERRUPT | SIG_SUSPEND, MYRTOS_MAX_DELAY,
                                                SIGNAL_WAIT_ANY | SIGNAL_CLEAR_ON_EXIT);
    // 恢复 Shell 的终端焦点
    VTS_ReturnToRootFocus();

    if (received_signals & SIG_INTERRUPT) {
        MyRTOS_printf("^C\n");
        Platform_ProgramManager_Kill(instance->pid);
        Task_WaitSignal(SIG_CHILD_EXIT, MS_TO_TICKS(100), SIGNAL_WAIT_ANY | SIGNAL_CLEAR_ON_EXIT);
    } else if (received_signals & SIG_SUSPEND) {
        if (Platform_ProgramManager_Suspend(instance->pid) == 0) {
            MyRTOS_printf("\nSuspended [%d] %s\n", instance->pid, instance->def->name);
        }
    }

    // 关键: 在返回到 Shell 循环之前, 丢弃输入管道中可能残留的任何字符.
    // 这可以防止未发送的行缓冲内容被 Shell 误解析为命令.
    char dummy_char;
    while (Stream_Read(Stream_GetTaskStdIn(NULL), &dummy_char, 1, 0) > 0);
}

// 'jobs' 命令的访问者函数, 格式化并打印单个作业信息.
static bool print_job_visitor(const ProgramInstance_t *instance, void *arg) {
    (void) arg;
    const char *state_str = (instance->state == PROG_STATE_RUNNING) ? "Running" : "Suspended";
    MyRTOS_printf("%-4d | %-12s | %s\n", instance->pid, state_str, instance->def->name);
    return true; // 继续遍历.
}

// "jobs" 命令: 列出所有作业.
static int cmd_jobs(ShellHandle_t shell_h, int argc, char *argv[]) {
    (void) shell_h;
    (void) argc;
    (void) argv;
    MyRTOS_printf("PID  | STATUS       | NAME\n");
    MyRTOS_printf("-----|--------------|----------------\n");
    Platform_ProgramManager_TraverseInstances(print_job_visitor, NULL);
    return 0;
}

// "run" 命令: 运行一个程序 (支持前台/后台模式).
static int cmd_run(ShellHandle_t shell_h, int argc, char *argv[]) {
    (void) shell_h;
    if (argc < 2) {
        MyRTOS_printf("Usage: run <program_name> [args...] [&]\n");
        return -1;
    }

    bool is_background = (strcmp(argv[argc - 1], "&") == 0);
    int prog_argc = (is_background ? argc - 2 : argc - 1);
    char **prog_argv = &argv[1];
    const char *prog_name = argv[1];

    if (is_background) {
        ProgramInstance_t *instance =
                Platform_ProgramManager_Run(prog_name, prog_argc, prog_argv, PROG_MODE_BACKGROUND);
        if (instance) {
            MyRTOS_printf("[%d] %s\n", instance->pid, prog_name);
        } else {
            MyRTOS_printf("Error: Failed to start program '%s' in background.\n", prog_name);
        }
    } else {
        ProgramInstance_t *instance =
                Platform_ProgramManager_Run(prog_name, prog_argc, prog_argv, PROG_MODE_FOREGROUND);
        if (instance) {
            manage_foreground_session(instance);
        } else {
            MyRTOS_printf("Error: Failed to start program '%s' in foreground.\n", prog_name);
        }
    }
    return 0;
}

// "kill" 命令: 终止一个作业.
static int cmd_kill(ShellHandle_t shell_h, int argc, char *argv[]) {
    (void) shell_h;
    if (argc != 2) {
        MyRTOS_printf("Usage: kill <pid>\n");
        return -1;
    }

    int pid = atoi(argv[1]);
    if (pid <= 0) {
        MyRTOS_printf("Error: Invalid PID.\n");
        return -1;
    }

    if (Platform_ProgramManager_Kill(pid) == 0) {
        MyRTOS_printf("Kill signal sent to PID %d.\n", pid);
    } else {
        MyRTOS_printf("Error: Job with PID %d not found.\n", pid);
    }

    return 0;
}

// "fg" 命令: 将作业切换到前台.
static int cmd_fg(ShellHandle_t shell_h, int argc, char *argv[]) {
    (void) shell_h;
    if (argc != 2) {
        MyRTOS_printf("Usage: fg <pid>\n");
        return -1;
    }

    int pid = atoi(argv[1]);
    if (pid <= 0) {
        MyRTOS_printf("Error: Invalid PID.\n");
        return -1;
    }

    // 查找实例, 但不直接使用返回的指针, 因为它可能在manage_foreground_session中失效.
    ProgramInstance_t *instance = Platform_ProgramManager_GetInstance(pid);
    if (instance == NULL) {
        MyRTOS_printf("fg: job not found: %d\n", pid);
        return -1;
    }

    MyRTOS_printf("%s\n", instance->def->name);
    if (instance->stdin_pipe == NULL) {
        instance->stdin_pipe = Pipe_Create(VTS_PIPE_BUFFER_SIZE);
        instance->stdout_pipe = Pipe_Create(VTS_PIPE_BUFFER_SIZE);
        if (!instance->stdin_pipe || !instance->stdout_pipe) {
            if (instance->stdin_pipe) Pipe_Delete(instance->stdin_pipe);
            if (instance->stdout_pipe) Pipe_Delete(instance->stdout_pipe);
            MyRTOS_printf("Error: failed to create pipes for fg.\n");
            return -1;
        }
        //将任务的IO重定向到新创建的管道.
        Stream_SetTaskStdIn(instance->task_handle, instance->stdin_pipe);
        Stream_SetTaskStdOut(instance->task_handle, instance->stdout_pipe);
        Stream_SetTaskStdErr(instance->task_handle, instance->stdout_pipe);
    }

    MyRTOS_printf("%s\n", instance->def->name);

    if (instance->state == PROG_STATE_SUSPENDED) {
        Platform_ProgramManager_Resume(pid);
    }

    instance->mode = PROG_MODE_FOREGROUND;

    manage_foreground_session(instance);

    return 0;
}

// "bg" 命令: 在后台恢复一个挂起的作业.
static int cmd_bg(ShellHandle_t shell_h, int argc, char *argv[]) {
    (void) shell_h;
    if (argc != 2) {
        MyRTOS_printf("Usage: bg <pid>\n");
        return -1;
    }

    int pid = atoi(argv[1]);
    if (pid <= 0) {
        MyRTOS_printf("Error: Invalid PID.\n");
        return -1;
    }

    ProgramInstance_t *instance = Platform_ProgramManager_GetInstance(pid);
    if (instance == NULL) {
        MyRTOS_printf("bg: job not found: %d\n", pid);
        return -1;
    }

    if (instance->state != PROG_STATE_SUSPENDED) {
        MyRTOS_printf("bg: job %d is already running.\n", pid);
        return -1;
    }

    if (Platform_ProgramManager_Resume(pid) == 0) {
        MyRTOS_printf("[%d] %s &\n", pid, instance->def->name);
    } else {
        // This case should ideally not happen if checks above passed.
        MyRTOS_printf("bg: failed to resume job %d.\n", pid);
    }

    return 0;
}

#endif // PLATFORM_USE_PROGRAM_MANGE
#endif // PLATFORM_USE_DEFAULT_COMMANDS
#endif // MYRTOS_SERVICE_SHELL_ENABLE
