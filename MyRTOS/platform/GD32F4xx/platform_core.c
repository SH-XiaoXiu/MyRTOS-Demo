//
// Created by XiaoXiu on 8/31/2025.
//

#include "gd32f4xx.h"
#include "gd32f4xx_misc.h"
#include "gd32f4xx_rcu.h"
#include "platform.h"

#include "MyRTOS.h"


#include "MyRTOS_Port.h"

//==============================================================================
// 模块导入
//==============================================================================
#if MYRTOS_SERVICE_IO_ENABLE == 1
#include <string.h>
#include "MyRTOS_IO.h"
#endif


#if MYRTOS_SERVICE_LOG_ENABLE == 1
#include "MyRTOS_Log.h"
#endif


#if MYRTOS_SERVICE_MONITOR_ENABLE == 1
#include "MyRTOS_Monitor.h"
#endif


#if MYRTOS_SERVICE_TIMER_ENABLE == 1
#include "MyRTOS_Timer.h"
#endif


#if MYRTOS_SERVICE_SHELL_ENABLE == 1
#include "MyRTOS_Shell.h"
#endif

#if MYRTOS_SERVICE_VTS_ENABLE == 1
#include "MyRTOS_VTS.h"
#endif


//==============================================================================
// 前置声明
//==============================================================================

//------------------------------------------------------------------------------
// 类型声明
//------------------------------------------------------------------------------

#if PLATFORM_USE_PROGRAM_MANGE == 1
/**
 * @brief 程序管理功能相关类型
 */
typedef int (*ProgramMain_t)(int argc, char *argv[]);

typedef struct {
    const char *name;
    const char *description;
    ProgramMain_t main_func;
} ProgramEntry_t;

typedef struct {
    ProgramMain_t main_func;
    int argc;
    char **argv;
    StreamHandle_t stdin_stream;
    StreamHandle_t stdout_stream;
    StreamHandle_t stderr_stream;
    StreamHandle_t stdout_pipe_to_delete;
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
#endif // PLATFORM_USE_PROGRAM_MANGE

//------------------------------------------------------------------------------
// 变量声明
//------------------------------------------------------------------------------

#if MYRTOS_SERVICE_IO_ENABLE == 1
/**
 * @brief IO及Shell服务相关外部变量
 */
extern StreamHandle_t g_system_stdin;
extern StreamHandle_t g_system_stdout;
extern StreamHandle_t g_system_stderr;
#endif // MYRTOS_SERVICE_IO_ENABLE

//------------------------------------------------------------------------------
// 函数声明
//------------------------------------------------------------------------------

/**
 * @brief 平台硬件与钩子函数
 */
#if PLATFORM_USE_CONSOLE == 1
void Platform_Console_HwInit(void);

void Platform_Console_OSInit(void);
#else
#define Platform_Console_HwInit()
#define Platform_Console_OSInit()
#endif // PLATFORM_USE_CONSOLE

#if PLATFORM_HIRES_TIMER_NUM == 1
void Platform_HiresTimer_Init(void);
#else
#define Platform_HiresTimer_Init()
#endif // PLATFORM_HIRES_TIMER_NUM

#if PLATFORM_USE_ERROR_HOOK == 1
void Platform_error_handler_init(void);
#else
#define Platform_error_handler_init()
#endif // PLATFORM_USE_ERROR_HOOK

#if PLATFORM_USE_DEFAULT_COMMANDS == 1
void platform_register_default_commands(ShellHandle_t shell_h);
#else
#define platform_register_default_commands(shell_h)
#endif // PLATFORM_USE_DEFAULT_COMMANDS

/**
 * @brief 程序管理功能函数
 */
#if PLATFORM_USE_PROGRAM_MANGE == 1
void ProgramRegistry_Init(ProgramRegistry_t *reg);

int ProgramRegistry_Register(ProgramRegistry_t *reg, const char *name, TaskHandle_t handle, LaunchInfo_t *info);

TaskHandle_t ProgramRegistry_Find(ProgramRegistry_t *reg, const char *name);

LaunchInfo_t *ProgramRegistry_Unregister(ProgramRegistry_t *reg, TaskHandle_t handle);

void ProgramRegistry_SignalShutdown(ProgramRegistry_t *reg, TaskHandle_t handle);

bool Program_ShouldShutdown(void);

void platform_on_back_command(void);

static void cleanup_spy_state(void);
#else
#define ProgramRegistry_Init(reg)
#define ProgramRegistry_Register(reg, name, handle, info) (-1)
#define ProgramRegistry_Find(reg, name) (NULL)
#define ProgramRegistry_Unregister(reg, handle) (NULL)
#define ProgramRegistry_SignalShutdown(reg, handle)
#define Program_ShouldShutdown() (false)
#define platform_on_back_command()
#define cleanup_spy_state()
#endif // PLATFORM_USE_PROGRAM_MANGE


/**
 * @brief Shell命令及示例程序函数
 */
#if PLATFORM_USE_DEFAULT_COMMANDS
// Shell命令函数
int cmd_run(ShellHandle_t shell, int argc, char *argv[]);

int cmd_kill(ShellHandle_t shell, int argc, char *argv[]);

int cmd_shell(ShellHandle_t shell, int argc, char *argv[]);

int cmd_log(ShellHandle_t shell, int argc, char *argv[]);

int cmd_logall(ShellHandle_t shell, int argc, char *argv[]);

int cmd_ls(ShellHandle_t shell, int argc, char *argv[]);

// 示例程序入口
int app_hello_main(int argc, char *argv[]);

int app_counter_main(int argc, char *argv[]);
#else
#define cmd_run(shell, argc, argv) (0)
#define cmd_kill(shell, argc, argv) (0)
#define cmd_shell(shell, argc, argv) (0)
#define cmd_log(shell, argc, argv) (0)
#define cmd_logall(shell, argc, argv) (0)
#define cmd_ls(shell, argc, argv) (0)
#define app_hello_main(argc, argv) (0)
#define app_counter_main(argc, argv) (0)
#endif // PLATFORM_USE_DEFAULT_COMMANDS


//==============================================================================
// 实现
//==============================================================================

//------------------------------------------------------------------------------
// 变量定义
//------------------------------------------------------------------------------

#if MYRTOS_SERVICE_IO_ENABLE == 1
/**
 * @brief IO及Shell服务相关全局/静态变量
 */
ShellHandle_t g_platform_shell_handle = NULL;
static StreamHandle_t g_original_task_stdout = NULL;
static StreamHandle_t g_spy_pipe = NULL;
#endif // MYRTOS_SERVICE_IO_ENABLE


#if PLATFORM_USE_PROGRAM_MANGE == 1
/**
 * @brief 程序管理功能相关全局/静态变量
 */
ProgramRegistry_t g_program_registry;
static TaskHandle_t g_current_foreground_task = NULL;
static TaskHandle_t g_spied_task = NULL;

const ProgramEntry_t g_program_table[] = {
    {"hello", "输出HelloWorld", app_hello_main},
    {"counter", "一个计数程序", app_counter_main,},
    {NULL, NULL}
};
#endif // PLATFORM_USE_PROGRAM_MANGE


//------------------------------------------------------------------------------
// 函数
//------------------------------------------------------------------------------

#if PLATFORM_USE_PROGRAM_MANGE == 1
/**
 * @brief 程序管理功能实现
 */
void ProgramRegistry_Init(ProgramRegistry_t *reg) {
    if (!reg)
        return;
    ProgramRegistryInternal_t *internal = MyRTOS_Malloc(sizeof(ProgramRegistryInternal_t));
    if (!internal) {
        while (1);
    }
    memset(internal, 0, sizeof(ProgramRegistryInternal_t));
    internal->mutex = Mutex_Create();
    if (!internal->mutex) {
        MyRTOS_Free(internal);
        while (1);
    }
    reg->_internal = internal;
}

static char *my_strdup(const char *s) {
    if (!s)
        return NULL;
    size_t len = strlen(s) + 1;
    char *new_s = MyRTOS_Malloc(len);
    if (new_s)
        memcpy(new_s, s, len);
    return new_s;
}

int ProgramRegistry_Register(ProgramRegistry_t *reg, const char *name, TaskHandle_t handle, LaunchInfo_t *info) {
    ProgramRegistryInternal_t *internal = (ProgramRegistryInternal_t *) reg->_internal;
    Mutex_Lock(internal->mutex);
    if (internal->count >= MAX_REGISTERED_PROGRAMS) {
        Mutex_Unlock(internal->mutex);
        return -1;
    }
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
    internal->entries[index].name = my_strdup(name);
    if (!internal->entries[index].name) {
        Mutex_Unlock(internal->mutex);
        return -1;
    }
    internal->entries[index].handle = handle;
    internal->entries[index].shutdown_requested = false;
    internal->entries[index].launch_info = info; // 绑定资源
    internal->count++;
    Mutex_Unlock(internal->mutex);
    return 0;
}

TaskHandle_t ProgramRegistry_Find(ProgramRegistry_t *reg, const char *name) {
    ProgramRegistryInternal_t *internal = (ProgramRegistryInternal_t *) reg->_internal;
    TaskHandle_t found_handle = NULL;
    Mutex_Lock(internal->mutex);
    for (int i = 0; i < MAX_REGISTERED_PROGRAMS; ++i) {
        if (internal->entries[i].handle != NULL && strcmp(internal->entries[i].name, name) == 0) {
            found_handle = internal->entries[i].handle;
            break;
        }
    }
    Mutex_Unlock(internal->mutex);
    return found_handle;
}

/**
 * @brief 从注册表中移除一个任务条目，并返回其关联的资源指针
 * @return LaunchInfo_t* 指向需要被清理的资源，如果未找到则返回NULL
 */
LaunchInfo_t *ProgramRegistry_Unregister(ProgramRegistry_t *reg, TaskHandle_t handle) {
    ProgramRegistryInternal_t *internal = (ProgramRegistryInternal_t *) reg->_internal;
    LaunchInfo_t *info_to_return = NULL;
    Mutex_Lock(internal->mutex);
    for (int i = 0; i < MAX_REGISTERED_PROGRAMS; ++i) {
        if (internal->entries[i].handle == handle) {
            MyRTOS_Free(internal->entries[i].name);
            info_to_return = internal->entries[i].launch_info;
            internal->entries[i].name = NULL;
            internal->entries[i].handle = NULL;
            internal->entries[i].shutdown_requested = false;
            internal->entries[i].launch_info = NULL;
            internal->count--;
            break;
        }
    }
    Mutex_Unlock(internal->mutex);
    return info_to_return;
}

void ProgramRegistry_SignalShutdown(ProgramRegistry_t *reg, TaskHandle_t handle) {
    ProgramRegistryInternal_t *internal = (ProgramRegistryInternal_t *) reg->_internal;
    Mutex_Lock(internal->mutex);
    for (int i = 0; i < MAX_REGISTERED_PROGRAMS; ++i) {
        if (internal->entries[i].handle == handle) {
            internal->entries[i].shutdown_requested = true;
            break;
        }
    }
    Mutex_Unlock(internal->mutex);
}

// 辅助函数
static void cleanup_launch_info(LaunchInfo_t *info) {
    if (!info)
        return;
    for (int i = 0; i < info->argc; ++i) {
        MyRTOS_Free(info->argv[i]);
    }
    MyRTOS_Free(info->argv);
    if (info->stdout_pipe_to_delete) {
        Pipe_Delete(info->stdout_pipe_to_delete);
    }
    MyRTOS_Free(info);
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

/**
 * @brief 任务清理函数
 */
static void cleanup_task_resources(TaskHandle_t task_to_clean) {
    if (!task_to_clean)
        return;

    MyRTOS_printf("\n[Platform] 正在清理任务资源: %s\n", Task_GetName(task_to_clean));

    // 从注册表中原子地移除任务并获取其资源指针
    LaunchInfo_t *info = ProgramRegistry_Unregister(&g_program_registry, task_to_clean);

    // 尝试优雅关闭
    ProgramRegistry_SignalShutdown(&g_program_registry, task_to_clean);
    Task_Delay(MS_TO_TICKS(100));

    // 如果任务仍在，强制删除
    if (Task_GetState(task_to_clean) != TASK_STATE_UNUSED) {
        MyRTOS_printf("[Platform] 任务未正常退出 强制删除\n");
        Task_Delete(task_to_clean);
    }

    // 无论任务如何结束，都持有info指针并可以安全地清理资源
    cleanup_launch_info(info);
}

static void cleanup_foreground_task(void) {
    if (g_current_foreground_task) {
        TaskHandle_t task = g_current_foreground_task;
        g_current_foreground_task = NULL; // 立即清除
        cleanup_task_resources(task);
    }
}

void platform_on_back_command(void) {
    if (g_current_foreground_task)
        cleanup_foreground_task();
    else if (g_spied_task)
        cleanup_spy_state();
}

static void cleanup_spy_state() {
    if (g_spied_task) {
        Task_SetStdOut(g_spied_task, g_original_task_stdout);
    }
    if (g_spy_pipe) {
        Pipe_Delete(g_spy_pipe);
    }
    g_spied_task = NULL;
    g_original_task_stdout = NULL;
    g_spy_pipe = NULL;
}
#endif // PLATFORM_USE_PROGRAM_MANGE


#if PLATFORM_USE_DEFAULT_COMMANDS == 1
/**
 * @brief 示例程序实现
 */
int app_hello_main(int argc, char *argv[]) {
    MyRTOS_printf("Hello from 'hello' app!\n");
    return 0;
}

int app_counter_main(int argc, char *argv[]) {
    int i = 0;
    while (!Program_ShouldShutdown()) {
        MyRTOS_printf("Counter: %d\n", i++);
        Task_Delay(MS_TO_TICKS(1000));
    }
    MyRTOS_printf("Counter task shutting down gracefully.\n");
    return 0;
}
#endif // PLATFORM_USE_DEFAULT_COMMANDS


/**
 * @brief 平台生命周期函数 (初始化与启动)
 */
static void system_clock_config(void) {
    rcu_deinit();
    rcu_osci_on(RCU_HXTAL);
    if (SUCCESS != rcu_osci_stab_wait(RCU_HXTAL)) {
        while (1);
    }
    rcu_ahb_clock_config(RCU_AHB_CKSYS_DIV1);
    rcu_apb2_clock_config(RCU_APB2_CKAHB_DIV2);
    rcu_apb1_clock_config(RCU_APB1_CKAHB_DIV4);
    uint32_t pll_m = 8, pll_n = 336, pll_p = 2, pll_q = 7;
    rcu_pll_config(RCU_PLLSRC_HXTAL, pll_m, pll_n, pll_p, pll_q);
    rcu_osci_on(RCU_PLL_CK);
    if (SUCCESS != rcu_osci_stab_wait(RCU_FLAG_PLLSTB)) {
        while (1);
    }
    rcu_system_clock_source_config(RCU_CKSYSSRC_PLLP);
    while (RCU_SCSS_PLLP != rcu_system_clock_source_get()) {
    }
}

void Platform_Init(void) {
    nvic_priority_group_set(NVIC_PRIGROUP_PRE4_SUB0);
    Platform_EarlyInit_Hook();
    Platform_Console_HwInit();
    Platform_HiresTimer_Init();
    Platform_BSP_Init_Hook();
    MyRTOS_Init();
#if PLATFORM_USE_PROGRAM_MANGE == 1
    ProgramRegistry_Init(&g_program_registry);
#endif
    Platform_error_handler_init();
#if (MYRTOS_SERVICE_IO_ENABLE == 1) && (MYRTOS_SERVICE_VTS_ENABLE == 1) && (MYRTOS_SERVICE_SHELL_ENABLE == 1)
    Platform_Console_OSInit();
    StdIOService_Init();
    StreamHandle_t shell_input_pipe = Pipe_Create(VTS_PIPE_BUFFER_SIZE);
    StreamHandle_t shell_output_pipe = Pipe_Create(VTS_PIPE_BUFFER_SIZE);
    if (!shell_input_pipe || !shell_output_pipe) {
        while (1);
    }
    VTS_Config_t v_config = {
        .physical_stream = Platform_Console_GetStream(),
        .root_input_stream = shell_input_pipe,
        .root_output_stream = shell_output_pipe,
        .back_command_sequence = "back\r\n",
        .back_command_len = strlen("back\r\n"),
        .on_back_command = platform_on_back_command
    };
    if (VTS_Init(&v_config) != 0) {
        while (1);
    }
    g_system_stdout = VTS_GetBackgroundStream();
    g_system_stderr = VTS_GetBackgroundStream();
    ShellConfig_t s_config = {.prompt = "MyRTOS> "};
    g_platform_shell_handle = Shell_Init(&s_config);
    if (g_platform_shell_handle) {
        TaskHandle_t shell_task_h = Shell_Start(g_platform_shell_handle, "Shell", 4, 4096);
        if (shell_task_h) {
            Task_SetStdIn(shell_task_h, shell_input_pipe);
            Task_SetStdOut(shell_task_h, shell_output_pipe);
            Task_SetStdErr(shell_task_h, shell_output_pipe);
#if PLATFORM_USE_DEFAULT_COMMANDS == 1
            platform_register_default_commands(g_platform_shell_handle);
            Shell_RegisterCommand(g_platform_shell_handle, "run", "运行一个程序", cmd_run);
            Shell_RegisterCommand(g_platform_shell_handle, "kill", "杀死一个程序", cmd_kill);
            Shell_RegisterCommand(g_platform_shell_handle, "shell", "切换到shell", cmd_shell);
            Shell_RegisterCommand(g_platform_shell_handle, "log", "监听日志", cmd_log);
            Shell_RegisterCommand(g_platform_shell_handle, "logall", "查看所有日志", cmd_logall);
            Shell_RegisterCommand(g_platform_shell_handle, "ls", "列出所有程序", cmd_ls);
#endif // PLATFORM_USE_DEFAULT_COMMANDS
        }
    }
#endif // IO, VTS, SHELL enabled
#if MYRTOS_SERVICE_LOG_ENABLE == 1
    Log_Init();
#endif
#if MYRTOS_SERVICE_MONITOR_ENABLE == 1
    MonitorConfig_t m_config = {.get_hires_timer_value = Platform_Timer_GetHiresValue};
    Monitor_Init(&m_config);
#endif
#if MYRTOS_SERVICE_TIMER_ENABLE == 1
    TimerService_Init(MYRTOS_MAX_PRIORITIES - 2, 2048);
#endif
    Platform_AppSetup_Hook(g_platform_shell_handle);
}

void Platform_StartScheduler(void) {
    Platform_CreateTasks_Hook();
    Task_StartScheduler(Platform_IdleTask_Hook);
    while (1);
}


#if PLATFORM_USE_DEFAULT_COMMANDS == 1
/**
 * @brief Shell 命令实现
 */
static void program_task_entry(void *param) {
    LaunchInfo_t *info = (LaunchInfo_t *) param;
    TaskHandle_t self = Task_GetCurrentTaskHandle();
    Task_SetStdIn(self, info->stdin_stream);
    Task_SetStdOut(self, info->stdout_stream);
    Task_SetStdErr(self, info->stderr_stream);

    info->main_func(info->argc, info->argv);

    // 正常退出清理路径
    ProgramRegistry_Unregister(&g_program_registry, self); // 注销自己
    if (self == g_current_foreground_task) {
        g_current_foreground_task = NULL;
        VTS_ReturnToRootFocus();
    }
    cleanup_launch_info(info); // 清理自己的资源
    Task_Delete(NULL); // 自我销毁
}

int cmd_run(ShellHandle_t shell, int argc, char *argv[]) {
    if (argc < 2) {
        MyRTOS_printf("用法: run <程序名> [参数] [-d]\n");
        return -1;
    }
    bool detached = (argc > 2 && strcmp(argv[argc - 1], "-d") == 0);
    int prog_argc = detached ? argc - 2 : argc - 1;
    const char *prog_name = argv[1];
    ProgramMain_t main_func = NULL;
    for (int i = 0; g_program_table[i].name != NULL; ++i)
        if (strcmp(g_program_table[i].name, prog_name) == 0)
            main_func = g_program_table[i].main_func;
    if (!main_func) {
        MyRTOS_printf("错误: 未找到程序 '%s'。\n", prog_name);
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
        MyRTOS_printf("错误: 内存分配失败。\n");
        cleanup_launch_info(launch_info);
        return -1;
    }
    for (int i = 0; i < prog_argc; ++i)
        launch_info->argv[i] = my_strdup(argv[i + 1]);

    TaskHandle_t new_task_h = NULL;
    if (detached) {
        launch_info->stdin_stream = NULL;
        launch_info->stdout_stream = g_system_stdout;
        launch_info->stderr_stream = g_system_stderr;
        new_task_h = Task_Create(program_task_entry, prog_name, PLATFORM_PROGRAM_LAUNCH_STACK, launch_info, 3);
    } else {
        cleanup_foreground_task();
        cleanup_spy_state();
        StreamHandle_t prog_output = Pipe_Create(VTS_PIPE_BUFFER_SIZE);
        if (!prog_output) {
            MyRTOS_printf("错误: 创建管道失败。\n");
            cleanup_launch_info(launch_info);
            return -1;
        }
        launch_info->stdin_stream = NULL;
        launch_info->stdout_stream = prog_output;
        launch_info->stderr_stream = prog_output;
        launch_info->stdout_pipe_to_delete = prog_output;
        new_task_h = Task_Create(program_task_entry, prog_name, PLATFORM_PROGRAM_LAUNCH_STACK, launch_info, 3);
        if (new_task_h) {
            g_current_foreground_task = new_task_h;
            VTS_SetFocus(prog_output);
        } else {
            cleanup_launch_info(launch_info);
            launch_info = NULL;
        }
    }

    if (!new_task_h) {
        MyRTOS_printf("错误: 创建任务失败。\n");
        if (launch_info) {
            cleanup_launch_info(launch_info);
        }
        return -1;
    }

    ProgramRegistry_Register(&g_program_registry, prog_name, new_task_h, launch_info);
    MyRTOS_printf("已启动%s任务 '%s' (句柄: %p)\n", detached ? "后台" : "前台", prog_name, new_task_h);
    return 0;
}

int cmd_kill(ShellHandle_t shell, int argc, char *argv[]) {
    if (argc != 2) {
        MyRTOS_printf("用法: kill <任务名>\n");
        return -1;
    }
    TaskHandle_t task_to_kill = ProgramRegistry_Find(&g_program_registry, argv[1]);
    if (!task_to_kill) {
        MyRTOS_printf("错误: 未找到任务 '%s'。\n", argv[1]);
        return -1;
    }

    if (task_to_kill == g_current_foreground_task) {
        VTS_ReturnToRootFocus();
        cleanup_foreground_task();
    } else {
        cleanup_task_resources(task_to_kill);
    }
    return 0;
}

int cmd_shell(ShellHandle_t shell, int argc, char *argv[]) {
    cleanup_spy_state();
    cleanup_foreground_task();
    VTS_ReturnToRootFocus();
    MyRTOS_printf("Focus returned to Shell.\n");
    return 0;
}

int cmd_log(ShellHandle_t shell, int argc, char *argv[]) {
    if (argc != 2) {
        MyRTOS_printf("Usage: log <task_name>\n");
        return -1;
    }
    cleanup_spy_state();
    cleanup_foreground_task();
    TaskHandle_t target_task = Task_FindByName(argv[1]);
    if (!target_task) {
        MyRTOS_printf("Task '%s' not found.\n", argv[1]);
        return -1;
    }
    g_spy_pipe = Pipe_Create(VTS_PIPE_BUFFER_SIZE);
    if (!g_spy_pipe) {
        MyRTOS_printf("Error: Failed to create spy pipe.\n");
        return -1;
    }
    MyRTOS_Port_EnterCritical();
    g_original_task_stdout = Task_GetStdOut(target_task);
    Task_SetStdOut(target_task, g_spy_pipe);
    g_spied_task = target_task;
    MyRTOS_Port_ExitCritical();
    MyRTOS_printf("Spying on task '%s'. Type 'back' or 'shell' to return.\n", argv[1]);
    VTS_SetFocus(g_spy_pipe);
    return 0;
}

int cmd_logall(ShellHandle_t shell, int argc, char *argv[]) {
    if (argc != 2) {
        MyRTOS_printf("用法：logall <on|off>\n");
        return -1;
    }
    if (strcmp(argv[1], "on") == 0) {
        VTS_SetLogAllMode(true);
        MyRTOS_printf("已启用 LogAll 模式。\n");
    } else if (strcmp(argv[1], "off") == 0) {
        VTS_SetLogAllMode(false);
        MyRTOS_printf("已禁用 LogAll 模式。\n");
    } else {
        MyRTOS_printf("无效参数。请使用 'on' 或 'off'。\n");
    }
    return 0;
}

int cmd_ls(ShellHandle_t shell, int argc, char *argv[]) {
    MyRTOS_printf("可用程序：\n");
    for (int i = 0; g_program_table[i].name != NULL; ++i)
        MyRTOS_printf("%s - %s\n", g_program_table[i].name, g_program_table[i].description);
    return 0;
}
#endif // PLATFORM_USE_DEFAULT_COMMANDS
