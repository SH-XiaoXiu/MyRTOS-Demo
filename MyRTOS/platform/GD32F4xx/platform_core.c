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

#if MYRTOS_SERVICE_PROCESS_ENABLE == 1
    Process_Init();
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
        while (1); // 致命错误
    }

    VTS_Config_t v_config = {
        .physical_stream = Platform_Console_GetStream(),
        .root_input_stream = vts_pipe_in,
        .root_output_stream = vts_pipe_out,
        .signal_receiver_task_handle = NULL  // 由用户程序设置
    };

    if (VTS_Init(&v_config) != 0) {
        while (1);
    }

    // VTS 启动后接管后台日志输出流
    g_system_stdout = VTS_GetBackgroundStream();
    g_system_stderr = VTS_GetBackgroundStream();
#endif

#if MYRTOS_SERVICE_MONITOR_ENABLE == 1
    MonitorConfig_t m_config = {.get_hires_timer_value = Platform_Timer_GetHiresValue};
    Monitor_Init(&m_config);
#endif

#if MYRTOS_SERVICE_TIMER_ENABLE == 1
    TimerService_Init(MYRTOS_MAX_PRIORITIES - 2, 2048);
#endif

    // 调用应用设置钩子
    Platform_AppSetup_Hook();
}

void Platform_StartScheduler(void) {
    Platform_CreateTasks_Hook();
    Task_StartScheduler(Platform_IdleTask_Hook);
    while (1) {
    };
}

void Platform_Reboot(void) {
    NVIC_SystemReset();
}
