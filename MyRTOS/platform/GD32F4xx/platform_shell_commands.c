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
#include "gd32f4xx.h"
#include "MyRTOS_Log.h"
#include "platform_gd32_console.h"

extern ShellHandle_t g_platform_shell_handle;
extern TaskHandle_t g_shell_task_h;


#if MYRTOS_SERVICE_VTS_ENABLE == 1
#include "MyRTOS_VTS.h"
#define ToRootFocus()   VTS_ReturnToRootFocus()
#define SetFocus(input_stream,  output_stream)   VTS_SetFocus(input_stream,  output_stream)
#else
// 空实现
static inline void ReturnToRootFocus(void) {
}

static inline int SetFocus(StreamHandle_t input_stream, StreamHandle_t output_stream) {
    return 0;
}

#endif


//======================================
// 平台程序管理
//======================================
#if PLATFORM_USE_PROGRAM_MANGE == 1


// 类型声明
typedef struct {
    ProgramMain_t main_func;
    int argc;
    char **argv;
    StreamHandle_t stdin_stream;
    StreamHandle_t stdout_stream;
    StreamHandle_t stderr_stream;
    StreamHandle_t stdout_pipe_to_delete;
    StreamHandle_t stdin_pipe_to_delete;
} LaunchInfo_t;

typedef struct {
    char *name;
    TaskHandle_t handle;
    volatile bool shutdown_requested;
    LaunchInfo_t *launch_info;
} ProgramInfo_t;

typedef struct {
    ProgramInfo_t entries[MAX_REGISTERED_PROGRAMS];
    int count;
    MutexHandle_t mutex;
} ProgramRegistryInternal_t;

typedef struct {
    void *_internal;
} ProgramRegistry_t;

// 变量定义
static ProgramRegistry_t g_program_registry;
static TaskHandle_t g_current_foreground_task = NULL;

// 前置声明
int app_hello_main(int argc, char *argv[]);

int app_counter_main(int argc, char *argv[]);

static void cleanup_launch_info(LaunchInfo_t *info);

// 全局程序表
const ProgramEntry_t g_program_table[] = {
    {"hello", "输出HelloWorld", app_hello_main},
    {"counter", "一个计数程序", app_counter_main,},
    {NULL, NULL, NULL} // 必须以NULL结尾
};

// 内部辅助函数
static char *my_strdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char *new_s = MyRTOS_Malloc(len);
    if (new_s) memcpy(new_s, s, len);
    return new_s;
}

static int ProgramRegistry_Register(TaskHandle_t handle, LaunchInfo_t *info) {
    ProgramRegistryInternal_t *internal = (ProgramRegistryInternal_t *) g_program_registry._internal;
    Mutex_Lock(internal->mutex);
    int index = -1;
    for (int i = 0; i < MAX_REGISTERED_PROGRAMS; ++i) {
        if (internal->entries[i].handle == NULL) {
            index = i;
            break;
        }
    }
    if (index == -1) {
        Mutex_Unlock(internal->mutex);
        return -1;
    }
    internal->entries[index].handle = handle;
    internal->entries[index].launch_info = info;
    Mutex_Unlock(internal->mutex);
    return 0;
}

static LaunchInfo_t *ProgramRegistry_Unregister(TaskHandle_t handle) {
    ProgramRegistryInternal_t *internal = (ProgramRegistryInternal_t *) g_program_registry._internal;
    LaunchInfo_t *info_to_return = NULL;
    Mutex_Lock(internal->mutex);
    for (int i = 0; i < MAX_REGISTERED_PROGRAMS; ++i) {
        if (internal->entries[i].handle == handle) {
            info_to_return = internal->entries[i].launch_info;
            memset(&internal->entries[i], 0, sizeof(ProgramInfo_t));
            break;
        }
    }
    Mutex_Unlock(internal->mutex);
    return info_to_return;
}

static TaskHandle_t ProgramRegistry_Find(const char *name) {
    return Task_FindByName(name);
}

static void ProgramRegistry_SignalShutdown(TaskHandle_t handle) {
    ProgramRegistryInternal_t *internal = (ProgramRegistryInternal_t *) g_program_registry._internal;
    Mutex_Lock(internal->mutex);
    for (int i = 0; i < MAX_REGISTERED_PROGRAMS; ++i) {
        if (internal->entries[i].handle == handle) {
            internal->entries[i].shutdown_requested = true;
            break;
        }
    }
    Mutex_Unlock(internal->mutex);
}

//API实现
void Platform_ProgramManager_Init(void) {
    ProgramRegistryInternal_t *internal = MyRTOS_Malloc(sizeof(ProgramRegistryInternal_t));
    if (!internal) { while (1); } // 致命错误
    memset(internal, 0, sizeof(ProgramRegistryInternal_t));
    internal->mutex = Mutex_Create();
    if (!internal->mutex) {
        MyRTOS_Free(internal);
        while (1);
    } // 致命错误
    g_program_registry._internal = internal;
}

bool Program_ShouldShutdown(void) {
    ProgramRegistryInternal_t *internal = (ProgramRegistryInternal_t *) g_program_registry._internal;
    TaskHandle_t self = Task_GetCurrentTaskHandle();
    bool should_shutdown = false;
    Mutex_Lock(internal->mutex);
    for (int i = 0; i < MAX_REGISTERED_PROGRAMS; ++i) {
        if (internal->entries[i].handle == self) {
            should_shutdown = internal->entries[i].shutdown_requested;
            break;
        }
    }
    Mutex_Unlock(internal->mutex);
    return should_shutdown;
}

// 示例程序
int app_hello_main(int argc, char *argv[]) {
    MyRTOS_printf("Hello from 'hello' app!\n");
    for (int i = 0; i < argc; ++i) {
        MyRTOS_printf("  argv[%d]: %s\n", i, argv[i]);
    }
    return 0;
}

int app_counter_main(int argc, char *argv[]) {
    int i = 0;
    MyRTOS_printf("Counter app started. Press Ctrl+C in shell to interrupt.\n");
    while (!Program_ShouldShutdown()) {
        MyRTOS_printf("Counter: %d\n", i++);
        Task_Delay(MS_TO_TICKS(1000));
    }
    MyRTOS_printf("Counter task shutting down gracefully.\n");
    return 0;
}

#endif // PLATFORM_USE_PROGRAM_MANGE


// Shell 命令实现
#if PLATFORM_USE_DEFAULT_COMMANDS == 1

// 前置声明
static int cmd_help(ShellHandle_t shell, int argc, char *argv[]);

static int cmd_reboot(ShellHandle_t shell, int argc, char *argv[]);

static int cmd_ps(ShellHandle_t shell, int argc, char *argv[]);

static int cmd_run(ShellHandle_t shell, int argc, char *argv[]);

static int cmd_kill(ShellHandle_t shell, int argc, char *argv[]);

static int cmd_ls(ShellHandle_t shell, int argc, char *argv[]);

static int cmd_logall(ShellHandle_t shell, int argc, char *argv[]);

static int cmd_log(ShellHandle_t shell_h, int argc, char *argv[]);

static int cmd_loglevel(ShellHandle_t shell_h, int argc, char *argv[]);

// 命令表
static const struct ShellCommandDef {
    const char *name;
    const char *help;
    ShellCommandCallback_t callback;
} g_platform_commands[] = {
    {"help", "显示所有可用命令", cmd_help},
    {"reboot", "重启系统", cmd_reboot},
    {"ps", "显示系统状态", cmd_ps},
#if PLATFORM_USE_PROGRAM_MANGE == 1
    {"run", "运行一个程序. 用法: run <prog> [args...]", cmd_run},
    {"kill", "终止一个程序. 用法: kill <prog>", cmd_kill},
    {"ls", "列出所有可执行程序", cmd_ls},
#endif
    {"logall", "切换全局日志. 用法: logall <on|off>", cmd_logall},
    {"log", "监听指定Tag日志. 用法: log <tag>", cmd_log},
    {"loglevel", "设置日志级别. 用法: loglevel <0-4>", cmd_loglevel},

    {NULL, NULL, NULL} // 必须以NULL结尾
};

//
// help, reboot, ps 命令实现
//
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

static int cmd_reboot(ShellHandle_t shell_h, int argc, char *argv[]) {
    (void) argc;
    (void) argv;
    MyRTOS_printf("System rebooting...\n");
    Task_Delay(MS_TO_TICKS(100));
    NVIC_SystemReset();
    return 0;
}

static int cmd_ps(ShellHandle_t shell_h, int argc, char *argv[]) {
#if MYRTOS_SERVICE_MONITOR_ENABLE == 1
    (void) argc;
    (void) argv;
    const int COL_WIDTH_NAME = 16;
    static const char *const taskStateToStr[] = {"Unused", "Ready", "Delayed", "Blocked"};
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
                      (unsigned)Task_GetId(s->task_handle), taskStateToStr[s->state], prio_str, stack_str,
                      s->total_runtime);
    }
    HeapStats_t heap;
    Monitor_GetHeapStats(&heap);
    MyRTOS_printf("Heap Info -> Total: %-5u | Free: %-5u | Min Ever Free: %-5u\n", (unsigned)heap.total_heap_size,
                  (unsigned)heap.free_bytes_remaining, (unsigned)heap.minimum_ever_free_bytes);
    return 0;
#else
    MyRTOS_printf("Monitor service is disabled.\n");
    return -1;
#endif
}

//
// logall 命令实现
//
int cmd_logall(ShellHandle_t shell_h, int argc, char *argv[]) {
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


int cmd_log(ShellHandle_t shell_h, int argc, char *argv[]) {
    if (argc != 2) {
        MyRTOS_printf("Usage: log <taskName_or_tag>\n");
        MyRTOS_printf("Monitors logs from a specific tag in the foreground.\n");
        return -1;
    }
    char *tag_to_monitor = argv[1];
    //创建一个 Pipe，用于将过滤后的日志显示在前台
    StreamHandle_t pipe = Pipe_Create(1024);
    if (!pipe) {
        MyRTOS_printf("Error: Failed to create pipe for logging.\n");
        return -1;
    }
    //添加一个临时的 只关心特定tag的日志监听器
    //这个监听器的输出目标是 Pipe 的写端
    LogListenerHandle_t listener_h = Log_AddListener(pipe, LOG_LEVEL_DEBUG, tag_to_monitor);
    if (!listener_h) {
        MyRTOS_printf("Error: Failed to add log listener.\n");
        Pipe_Delete(pipe);
        return -1;
    }
    //VTS 将焦点切换到 Pipe 的读端
    VTS_SetFocus(Task_GetStdIn(NULL), pipe);
    VTS_SetTerminalMode(VTS_MODE_RAW);
    Stream_Printf(pipe, "--- Start monitoring LOGs with tag '%s'. Press any key to stop. ---\n", tag_to_monitor);
    //等待用户按键退出
    char ch;
    Stream_Read(Task_GetStdIn(NULL), &ch, 1, MYRTOS_MAX_DELAY);
    //恢复一切
    Stream_Printf(pipe, "\n--- Stopped monitoring tag '%s'. ---\n", tag_to_monitor);
    Task_Delay(MS_TO_TICKS(10));
    //移除临时监听器
    Log_RemoveListener(listener_h);
    //归还焦点
    VTS_ReturnToRootFocus();
    // 销毁 Pipe
    Pipe_Delete(pipe);
    return 0;
}


int cmd_loglevel(ShellHandle_t shell_h, int argc, char *argv[]) {
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
    }
    return 0;
}


#if PLATFORM_USE_PROGRAM_MANGE == 1
// 程序管理命令 (run, kill, ls) 实现
static void cleanup_launch_info(LaunchInfo_t *info) {
    if (!info) return;
    for (int i = 0; i < info->argc; ++i) MyRTOS_Free(info->argv[i]);
    MyRTOS_Free(info->argv);
    if (info->stdout_pipe_to_delete) Pipe_Delete(info->stdout_pipe_to_delete);
    if (info->stdin_pipe_to_delete) Pipe_Delete(info->stdin_pipe_to_delete);
    MyRTOS_Free(info);
}


static void program_task_entry(void *param) {
    LaunchInfo_t *info = (LaunchInfo_t *) param;

    // 程序任务会继承正确的IO,这里无需再次设置
    // Task_SetStdIn(self, info->stdin_stream);
    // Task_SetStdOut(self, info->stdout_stream);
    // Task_SetStdErr(self, info->stderr_stream);

    // 执行程序主函数
    info->main_func(info->argc, info->argv);

    // 任务函数返回，内核会自动清理任务栈等资源。
    // LaunchInfo_t 等应用层资源由创建或终止它的命令负责清理。
}


int cmd_run(ShellHandle_t shell, int argc, char *argv[]) {
    if (argc < 2) {
        MyRTOS_printf("用法: run <程序名> [参数...] [-d]\n");
        MyRTOS_printf("  -d: 在后台运行程序\n");
        return -1;
    }

    bool is_background = false;
    int prog_argc = 0;
    char *prog_argv[SHELL_MAX_ARGS];

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-d") == 0) {
            is_background = true;
        } else {
            if (prog_argc < SHELL_MAX_ARGS) {
                prog_argv[prog_argc++] = argv[i];
            }
        }
    }

    if (prog_argc == 0) {
        MyRTOS_printf("错误: 未指定要运行的程序名。\n");
        return -1;
    }

    const char *prog_name = prog_argv[0]; // 这是临时的名字指针
    ProgramMain_t main_func = NULL;
    for (int i = 0; g_program_table[i].name != NULL; ++i) {
        if (strcmp(g_program_table[i].name, prog_name) == 0) {
            main_func = g_program_table[i].main_func;
            break;
        }
    }

    if (!main_func) {
        MyRTOS_printf("错误: 未找到程序 '%s'。\n", prog_name);
        return -1;
    }
    if (g_current_foreground_task && !is_background) {
        MyRTOS_printf("错误: 已有前台程序在运行。\n");
        return -1;
    }

    LaunchInfo_t *launch_info = MyRTOS_Malloc(sizeof(LaunchInfo_t));
    if (!launch_info) {
        MyRTOS_printf("错误: 内存分配失败。\n");
        return -1;
    }
    memset(launch_info, 0, sizeof(LaunchInfo_t));

    launch_info->main_func = main_func;
    launch_info->argc = prog_argc;
    launch_info->argv = MyRTOS_Malloc(sizeof(char *) * prog_argc);
    if (!launch_info->argv) {
        cleanup_launch_info(launch_info);
        return -1;
    }
    for (int i = 0; i < prog_argc; ++i) {
        launch_info->argv[i] = my_strdup(prog_argv[i]);
    }

    TaskHandle_t new_task_h = Task_Create(program_task_entry, prog_name, PLATFORM_PROGRAM_LAUNCH_STACK, launch_info, 3);

    if (!new_task_h) {
        MyRTOS_printf("错误: 创建任务失败。\n");
        cleanup_launch_info(launch_info);
        return -1;
    }
    ProgramRegistry_Register(new_task_h, launch_info);
    if (is_background) {
        Task_SetStdIn(new_task_h, NULL);
#if (MYRTOS_SERVICE_VTS_ENABLE == 1)
        Task_SetStdOut(new_task_h, VTS_GetBackgroundStream());
        Task_SetStdErr(new_task_h, VTS_GetBackgroundStream());
#else
        Task_SetStdOut(new_task_h, NULL);
        Task_SetStdErr(new_task_h, NULL);
#endif
        MyRTOS_printf("已启动后台任务 '%s' (句柄: %p)\n", prog_name, new_task_h);
        return 0;
    } else {
        // 前台任务
        StreamHandle_t prog_input = Pipe_Create(VTS_PIPE_BUFFER_SIZE);
        StreamHandle_t prog_output = Pipe_Create(VTS_PIPE_BUFFER_SIZE);
        if (!prog_input || !prog_output) {
            MyRTOS_printf("错误: 创建管道失败。\n");
            LaunchInfo_t *li = ProgramRegistry_Unregister(new_task_h);
            Task_Delete(new_task_h);
            cleanup_launch_info(li);
            if (prog_input) Pipe_Delete(prog_input);
            return -1;
        }
        launch_info->stdin_pipe_to_delete = prog_input;
        launch_info->stdout_pipe_to_delete = prog_output;

        Task_SetStdIn(new_task_h, prog_input);
        Task_SetStdOut(new_task_h, prog_output);
        Task_SetStdErr(new_task_h, prog_output);

        g_current_foreground_task = new_task_h;
        MyRTOS_printf("已启动前台任务 '%s' (句柄: %p)\n", prog_name, new_task_h);

#if (MYRTOS_SERVICE_VTS_ENABLE == 1)
        VTS_SetFocus(prog_input, prog_output);
        MyRTOS_printf(" Shell is monitoring. Press Ctrl+C to interrupt. \n");
#else
        MyRTOS_printf(" Program is running in non-interactive mode. \n");
#endif
        while (Task_GetState(g_current_foreground_task) != TASK_STATE_UNUSED) {
#if (MYRTOS_SERVICE_VTS_ENABLE == 1)
            uint32_t signal = Task_WaitSignal(SIG_INTERRUPT, MYRTOS_MAX_DELAY,SIGNAL_WAIT_ANY | SIGNAL_CLEAR_ON_EXIT);
            if (signal & SIG_INTERRUPT) {
                MyRTOS_printf("\n Shell received SIG_INTERRUPT! \n");
                ProgramRegistry_SignalShutdown(g_current_foreground_task);
                Task_Delay(MS_TO_TICKS(100));
                if (Task_GetState(g_current_foreground_task) != TASK_STATE_UNUSED) {
                    Task_Delete(g_current_foreground_task);
                }
                break;
            }
#else
            Task_Delay(MS_TO_TICKS(100));
#endif
        }

        MyRTOS_printf("\n Program session ended. Reclaiming focus. \n");
        LaunchInfo_t *remaining_info = ProgramRegistry_Unregister(g_current_foreground_task);
        cleanup_launch_info(remaining_info);
        g_current_foreground_task = NULL;
#if (MYRTOS_SERVICE_VTS_ENABLE == 1)
        VTS_ReturnToRootFocus();
#endif
        MyRTOS_printf("Focus returned to Shell.\n");
        return 0;
    }
}


int cmd_kill(ShellHandle_t shell, int argc, char *argv[]) {
    if (argc != 2) {
        MyRTOS_printf("用法: kill <任务名>\n");
        return -1;
    }

    const char *task_name = argv[1];
    TaskHandle_t task_to_kill = ProgramRegistry_Find(task_name);
    if (!task_to_kill) {
        MyRTOS_printf("错误: 未找到任务 '%s'\n", task_name);
        return -1;
    }
    if (task_to_kill == g_current_foreground_task) {
        MyRTOS_printf("错误: 前台任务请用 Ctrl+C\n");
        return -1;
    }

    // 发协作退出信号
    ProgramRegistry_SignalShutdown(task_to_kill);

    //等待任务自行退出
    const int timeout_ms = 500;
    int waited_ms = 0;
    const int interval_ms = 50;
    while (Task_GetState(task_to_kill) != TASK_STATE_UNUSED && waited_ms < timeout_ms) {
        Task_Delay(MS_TO_TICKS(interval_ms));
        waited_ms += interval_ms;
    }

    bool forced = false;
    if (Task_GetState(task_to_kill) != TASK_STATE_UNUSED) {
        forced = true;

        //  断开任务持有的所有 IO
        Task_SetStdIn(task_to_kill, NULL);
        Task_SetStdOut(task_to_kill, NULL);
        Task_SetStdErr(task_to_kill, NULL);

        //  强制删除
        Task_Delete(task_to_kill);
    }

    // 清理应用层资源
    LaunchInfo_t *info = ProgramRegistry_Unregister(task_to_kill);
    cleanup_launch_info(info);

    if (forced) {
        MyRTOS_printf("警告: 任务 '%s' 未响应，已强制终止\n", task_name);
    } else {
        MyRTOS_printf("任务 '%s' 已成功终止\n", task_name);
    }

    return 0;
}


int cmd_ls(ShellHandle_t shell, int argc, char *argv[]) {
    MyRTOS_printf("可用程序：\n");
    for (int i = 0; g_program_table[i].name != NULL; ++i) {
        MyRTOS_printf("  %-12s - %s\n", g_program_table[i].name, g_program_table[i].description);
    }
    return 0;
}
#endif

// 命令注册函数
void Platform_RegisterDefaultCommands(ShellHandle_t shell_h) {
    for (int i = 0; g_platform_commands[i].name != NULL; ++i) {
        Shell_RegisterCommand(shell_h, g_platform_commands[i].name, g_platform_commands[i].help,
                              g_platform_commands[i].callback);
    }
}

#endif
#endif
