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
static const char *const g_task_state_str[] = {"Unused", "Ready", "Delayed", "Blocked"};
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
static bool print_program_visitor(const ProgramInstance_t *instance, void *arg);

static int cmd_progs(ShellHandle_t shell_h, int argc, char *argv[]);

static int execute_foreground_program(const char *name, int argc, char *argv[]);

static int cmd_run(ShellHandle_t shell_h, int argc, char *argv[]);

static int cmd_kill(ShellHandle_t shell_h, int argc, char *argv[]);
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
    {"progs", "列出所有正在运行的程序", cmd_progs},
    {"run", "运行程序. 用法: run <prog> [&]", cmd_run},
    {"kill", "终止程序. 用法: kill <pid>", cmd_kill},
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
        VTS_SetLogAllMode(true);
        MyRTOS_printf("Global log monitoring enabled.\n");
    } else if (strcmp(argv[1], "off") == 0) {
        VTS_SetLogAllMode(false);
        MyRTOS_printf("Global log monitoring disabled.\n");
    } else {
        MyRTOS_printf("Invalid argument. Use 'on' or 'off'.\n");
        return -1;
    }
    return 0;
}

// "log" 命令: 监控指定标签的实时日志.
static int cmd_log(ShellHandle_t shell_h, int argc, char *argv[]) {
    (void) shell_h;
    if (argc != 2) {
        MyRTOS_printf("Usage: log <taskName_or_tag>\n");
        return -1;
    }

    // 创建管道用于日志流.
    StreamHandle_t pipe = Pipe_Create(1024);
    if (!pipe) {
        MyRTOS_printf("Error: Failed to create pipe for logging.\n");
        return -1;
    }

    // 添加临时监听器, 将指定 tag 的日志重定向到管道.
    LogListenerHandle_t listener_h = Log_AddListener(pipe, LOG_LEVEL_DEBUG, argv[1]);
    if (!listener_h) {
        MyRTOS_printf("Error: Failed to add log listener.\n");
        Pipe_Delete(pipe);
        return -1;
    }

    // 切换 VTS 焦点到日志管道, 并设置为原始模式.
    VTS_SetFocus(Task_GetStdIn(NULL), pipe);
    VTS_SetTerminalMode(VTS_MODE_RAW);
    Stream_Printf(pipe, "--- Start monitoring LOGs with tag '%s'. Press any key to stop. ---\n", argv[1]);

    // 等待任意按键输入以退出监控.
    char ch;
    Stream_Read(Task_GetStdIn(NULL), &ch, 1, MYRTOS_MAX_DELAY);
    Stream_Printf(pipe, "\n--- Stopped monitoring tag '%s'. ---\n", argv[1]);
    Task_Delay(MS_TO_TICKS(10));

    // 恢复 VTS 焦点并清理资源.
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

// 'progs' 命令的访问者函数, 格式化并打印单个程序实例信息.
static bool print_program_visitor(const ProgramInstance_t *instance, void *arg) {
    (void) arg;
    const char *mode_str = (instance->mode == PROG_MODE_FOREGROUND) ? "FG" : "BG";
    MyRTOS_printf("%-4d | %-4s | %s\n", instance->pid, mode_str, instance->def->name);
    return true; // 继续遍历.
}

// "progs" 命令: 列出所有正在运行的程序.
static int cmd_progs(ShellHandle_t shell_h, int argc, char *argv[]) {
    (void) shell_h;
    (void) argc;
    (void) argv;
    MyRTOS_printf("PID  | MODE | NAME\n");
    MyRTOS_printf("-----|------|----------------\n");
    Platform_ProgramManager_TraverseInstances(print_program_visitor, NULL);
    return 0;
}

// 执行一个前台程序, 包括 VTS 焦点切换和信号等待.
static int execute_foreground_program(const char *name, int argc, char *argv[]) {
    // 启动程序.
    ProgramInstance_t *instance = Platform_ProgramManager_Run(name, argc, argv, PROG_MODE_FOREGROUND);
    if (instance == NULL) {
        MyRTOS_printf("Error: Failed to start program '%s'.\n", name);
        return -1;
    }

    // 将虚拟终端焦点切换到新程序.
    VTS_SetFocus(instance->stdin_pipe, instance->stdout_pipe);

    // 等待子程序退出或用户中断信号.
    uint32_t received_signals = Task_WaitSignal(SIG_CHILD_EXIT | SIG_INTERRUPT, MYRTOS_MAX_DELAY,
                                                SIGNAL_WAIT_ANY | SIGNAL_CLEAR_ON_EXIT);

    if (received_signals & SIG_INTERRUPT) {
        MyRTOS_printf("^C\n");
        // 收到中断信号, 强制终止前台程序.
        Platform_ProgramManager_Kill(instance->pid);
        // 短暂延时以确保任务清理完成.
        Task_Delay(MS_TO_TICKS(10));
    } else if (received_signals & SIG_CHILD_EXIT) {
        // 收到子程序正常退出信号, 无需额外操作.
    }

    // 恢复 Shell 的终端焦点.
    VTS_ReturnToRootFocus();
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
            MyRTOS_printf("Started program '%s' in background with PID %d.\n", prog_name, instance->pid);
        } else {
            MyRTOS_printf("Failed to start program '%s'.\n", prog_name);
        }
    } else {
        execute_foreground_program(prog_name, prog_argc, prog_argv);
    }

    return 0;
}

// "kill" 命令: 终止一个正在运行的程序.
static int cmd_kill(ShellHandle_t shell_h, int argc, char *argv[]) {
    (void) shell_h;
    if (argc != 2) {
        MyRTOS_printf("Usage: kill <pid>\n");
        return -1;
    }

    int pid = atoi(argv[1]);
    if (pid <= 0) {
        MyRTOS_printf("Invalid PID.\n");
        return -1;
    }

    if (Platform_ProgramManager_Kill(pid) == 0) {
        MyRTOS_printf("Kill signal sent to PID %d.\n", pid);
    } else {
        MyRTOS_printf("Program with PID %d not found.\n", pid);
    }

    return 0;
}

#endif // PLATFORM_USE_PROGRAM_MANGE
#endif // PLATFORM_USE_DEFAULT_COMMANDS
#endif // MYRTOS_SERVICE_SHELL_ENABLE
