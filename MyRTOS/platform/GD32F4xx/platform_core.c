//
// Created by XiaoXiu on 8/31/2025.
//

#include "gd32f4xx.h"
#include "gd32f4xx_misc.h"
#include "gd32f4xx_rcu.h"
#include "platform.h"

#include "MyRTOS.h"

//==============================================================================
// 模块导入
//==============================================================================
#if MYRTOS_SERVICE_IO_ENABLE == 1
#include "MyRTOS_IO.h"
#endif

#if MYRTOS_SERVICE_ASYNC_IO_ENABLE == 1
#include "MyRTOS_AsyncIO.h"
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


#if PLATFORM_USE_PROGRAM_MANGE == 1
#include "platform_shell_commands.h"
#include "platform_program_manager.h"
#endif

//==============================================================================
// 前置声明
//==============================================================================
#if MYRTOS_SERVICE_IO_ENABLE == 1
extern StreamHandle_t g_system_stdin;
extern StreamHandle_t g_system_stdout;
extern StreamHandle_t g_system_stderr;
#endif

#if MYRTOS_SERVICE_SHELL_ENABLE == 1
ShellHandle_t g_platform_shell_handle = NULL;
TaskHandle_t g_shell_task_h = NULL;
#endif


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
    Platform_BSP_After_Hook();
    MyRTOS_Init();
    Platform_error_handler_init();
    Platform_Console_OSInit();
#if MYRTOS_SERVICE_IO_ENABLE == 1
    StdIOService_Init();
#endif
#if MYRTOS_SERVICE_ASYNC_IO_ENABLE == 1
    if (AsyncIOService_Init() != 0) {
        while (1);
    }
#endif

#if MYRTOS_SERVICE_LOG_ENABLE == 1
    Log_Init();
#endif
#if PLATFORM_USE_PROGRAM_MANGE == 1
    Platform_ProgramManager_Init();
#endif

#if MYRTOS_SERVICE_IO_ENABLE == 1
    g_system_stdin = Platform_Console_GetStream();
    g_system_stdout = Platform_Console_GetStream();
    g_system_stderr = Platform_Console_GetStream();
#endif
    // 初始化服务句柄
    StreamHandle_t shell_input_pipe = NULL;
    StreamHandle_t shell_output_pipe = NULL;
    // 默认将系统标准IO指向物理控制台，后续服务可能会覆盖此设置
#if MYRTOS_SERVICE_IO_ENABLE == 1
    g_system_stdin = Platform_Console_GetStream();
    g_system_stdout = Platform_Console_GetStream();
    g_system_stderr = Platform_Console_GetStream();
#endif
    // 初始化 Shell 服务 (如果启用)
    // Shell 的初始化独立于 VTS，在 VTS 之前完成，以获取任务句柄
#if (MYRTOS_SERVICE_SHELL_ENABLE == 1)
    ShellConfig_t s_config = {.prompt = "MyRTOS> "};
    g_platform_shell_handle = Shell_Init(&s_config);
    if (!g_platform_shell_handle) {
        while (1);
    }
    g_shell_task_h = Shell_Start(g_platform_shell_handle, "Shell", 4, 4096);
    if (!g_shell_task_h) {
        while (1);
    }
#endif
    // 初始化 VTS 服务 (如果启用)
    // VTS 依赖 IO 服务，并可选择性地使用 Shell 任务句柄
#if (MYRTOS_SERVICE_VTS_ENABLE == 1)
    shell_input_pipe = Pipe_Create(VTS_PIPE_BUFFER_SIZE);
    shell_output_pipe = Pipe_Create(VTS_PIPE_BUFFER_SIZE);
    if (!shell_input_pipe || !shell_output_pipe) {
        while (1);
    } // 致命错误

    VTS_Config_t v_config = {
        .physical_stream = Platform_Console_GetStream(),
        .root_input_stream = shell_input_pipe,
        .root_output_stream = shell_output_pipe,
        .signal_receiver_task_handle = g_shell_task_h // 若 Shell 禁用, 此处为 NULL
    };

    if (VTS_Init(&v_config) != 0) {
        while (1);
    }
    //VTS 启动后接管后台日志输出流
    g_system_stdout = VTS_GetBackgroundStream();
    g_system_stderr = VTS_GetBackgroundStream();

#endif

    //根据 VTS 是否启用，为 Shell 配置正确的 IO 流
#if (MYRTOS_SERVICE_SHELL_ENABLE == 1)
#if (MYRTOS_SERVICE_VTS_ENABLE == 1)
    //VTS 和 Shell 都启用，Shell 连接到 VTS 管道
    Stream_SetTaskStdIn(g_shell_task_h, shell_input_pipe);
    Stream_SetTaskStdOut(g_shell_task_h, shell_output_pipe);
    Stream_SetTaskStdErr(g_shell_task_h, shell_output_pipe);
#else
#if MYRTOS_SERVICE_LOG_ENABLE == 1
    // 将物理控制台注册为监听器
    Log_AddListener(Platform_Console_GetStream(), LOG_LEVEL_DEBUG);
#endif
    Stream_SetTaskStdIn(g_shell_task_h, g_system_stdin);
    Stream_SetTaskStdOut(g_shell_task_h, g_system_stdout);
    Stream_SetTaskStdErr(g_shell_task_h, g_system_stderr);
#endif
#endif

    // 初始化其他服务
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

    // 注册 Shell 命令并调用应用设置钩子
#if (MYRTOS_SERVICE_SHELL_ENABLE == 1)
#if PLATFORM_USE_PROGRAM_MANGE == 1
    Platform_RegisterDefaultCommands(g_platform_shell_handle);
#endif
    Platform_AppSetup_Hook(g_platform_shell_handle);
#else
    Platform_AppSetup_Hook(NULL); // Shell 禁用时传递 NULL
#endif
}

void Platform_StartScheduler(void) {
    Platform_CreateTasks_Hook();
    Task_StartScheduler(Platform_IdleTask_Hook);
    while (1) {
    };
}
