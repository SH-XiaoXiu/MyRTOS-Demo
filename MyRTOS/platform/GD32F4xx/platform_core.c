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

#if MYRTOS_SERVICE_VTS_ENABLE == 1
#include "MyRTOS_VTS.h"
#endif

#if MYRTOS_SERVICE_PROCESS_ENABLE == 1
#include "MyRTOS_Process.h"
#endif

//==============================================================================
// 前置声明
//==============================================================================
#if MYRTOS_SERVICE_IO_ENABLE == 1
extern StreamHandle_t g_system_stdin;
extern StreamHandle_t g_system_stdout;
extern StreamHandle_t g_system_stderr;
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

    // 初始化控制台硬件，从此可以输出日志
    Platform_Console_HwInit();
    StreamHandle_t console = Platform_Console_GetStream();

    // 打印系统横幅
    if (console) {
        Stream_Printf(console, "\r\n\r\nMyRTOS (built %s %s)\r\n", __DATE__, __TIME__);
        Stream_Printf(console, "Copyright (C) 2025 XiaoXiu\r\n\r\n");
    }

    // 平台硬件初始化
    Platform_HiresTimer_Init();
    Platform_BSP_Init_Hook();
    Platform_BSP_After_Hook();

    // 内核初始化
    if (console) {
        Stream_Printf(console, "[Kernel]\r\n");
    }

    MyRTOS_Init();

    if (console) {
        Stream_Printf(console, "  Heap: %lu KB available\r\n",
                     MYRTOS_MEMORY_POOL_SIZE / 1024);
        Stream_Printf(console, "  Scheduler: %d priorities, %d task slots, %lu Hz tick\r\n",
                     MYRTOS_MAX_PRIORITIES,
                     MYRTOS_MAX_CONCURRENT_TASKS,
                     MYRTOS_TICK_RATE_HZ);
    }

    Platform_error_handler_init();
    Platform_Console_OSInit();

    // 服务层初始化
    if (console) {
        Stream_Printf(console, "\r\n[Services]\r\n");
    }

#if MYRTOS_SERVICE_IO_ENABLE == 1
    StdIOService_Init();
    if (console) Stream_Printf(console, "  [OK] I/O streams\r\n");
#endif

#if MYRTOS_SERVICE_ASYNC_IO_ENABLE == 1
    if (AsyncIOService_Init() != 0) {
        if (console) Stream_Printf(console, "  [FAIL] Async I/O\r\n");
        while (1);
    }
    if (console) Stream_Printf(console, "  [OK] Async I/O (queue: %d, prio: %d)\r\n",
                              MYRTOS_ASYNCIO_QUEUE_LENGTH,
                              MYRTOS_ASYNCIO_TASK_PRIORITY);
#endif

#if MYRTOS_SERVICE_LOG_ENABLE == 1
    Log_Init();
    if (console) {
        const char *log_mode = MYRTOS_LOG_USE_ASYNC_OUTPUT ? "async" : "sync";
        Stream_Printf(console, "  [OK] Logger (%s mode)\r\n", log_mode);
    }
#endif

#if MYRTOS_SERVICE_PROCESS_ENABLE == 1
    Process_Init();
    if (console) Stream_Printf(console, "  [OK] Process manager (max: %d)\r\n",
                              MYRTOS_PROCESS_MAX_INSTANCES);
#endif

    // 默认将系统标准IO指向物理控制台
#if MYRTOS_SERVICE_IO_ENABLE == 1
    g_system_stdin = Platform_Console_GetStream();
    g_system_stdout = Platform_Console_GetStream();
    g_system_stderr = Platform_Console_GetStream();
#endif

    // 初始化 VTS 服务 (如果启用)
#if MYRTOS_SERVICE_VTS_ENABLE == 1
    StreamHandle_t vts_pipe_in = Pipe_Create(VTS_PIPE_BUFFER_SIZE);
    StreamHandle_t vts_pipe_out = Pipe_Create(VTS_PIPE_BUFFER_SIZE);
    if (!vts_pipe_in || !vts_pipe_out) {
        if (console) Stream_Printf(console, "  [FAIL] VTS (pipe allocation)\r\n");
        while (1);
    }

    VTS_Config_t v_config = {
        .physical_stream = Platform_Console_GetStream(),
        .root_input_stream = vts_pipe_in,
        .root_output_stream = vts_pipe_out,
        .signal_receiver_task_handle = NULL
    };

    if (VTS_Init(&v_config) != 0) {
        if (console) Stream_Printf(console, "  [FAIL] Virtual terminal\r\n");
        while (1);
    }

    // VTS 启动后接管后台日志输出流
    g_system_stdout = VTS_GetBackgroundStream();
    g_system_stderr = VTS_GetBackgroundStream();
    if (console) Stream_Printf(console, "  [OK] Virtual terminal\r\n");
#endif

#if MYRTOS_SERVICE_MONITOR_ENABLE == 1
    MonitorConfig_t m_config = {.get_hires_timer_value = Platform_Timer_GetHiresValue};
    Monitor_Init(&m_config);
    if (console) Stream_Printf(console, "  [OK] System monitor\r\n");
#endif

#if MYRTOS_SERVICE_TIMER_ENABLE == 1
    TimerService_Init(MYRTOS_MAX_PRIORITIES - 2, 2048);
    if (console) Stream_Printf(console, "  [OK] Software timers\r\n");
#endif

    // 应用层初始化
    if (console) {
        Stream_Printf(console, "\r\n[Application]\r\n");
    }

    Platform_AppSetup_Hook();

    if (console) {
        Stream_Printf(console, "  Setup complete\r\n");
    }
}

void Platform_StartScheduler(void) {
    StreamHandle_t console = Platform_Console_GetStream();

    if (console) {
        Stream_Printf(console, "\r\nCreating system tasks...\r\n");
    }

    Platform_CreateTasks_Hook();

    if (console) {
        Stream_Printf(console, "System tasks created\r\n");
        Stream_Printf(console, "\r\n");
        Stream_Printf(console, "========================================\r\n");
        Stream_Printf(console, "Starting scheduler...\r\n");
        Stream_Printf(console, "Entering multitasking mode\r\n");
        Stream_Printf(console, "========================================\r\n");
        Stream_Printf(console, "\r\n");
    }

    Task_StartScheduler(Platform_IdleTask_Hook);

    // 永远不会执行到这里
    while (1) {
    };
}

void Platform_Reboot(void) {
    NVIC_SystemReset();
}
