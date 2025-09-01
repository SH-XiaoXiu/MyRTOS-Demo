//
// Created by XiaoXiu on 8/31/2025.
//

#include "platform.h"
#include "gd32f4xx.h"
#include "gd32f4xx_misc.h"
#include "gd32f4xx_rcu.h"

#include "MyRTOS_IO.h"
#include "MyRTOS_Log.h"
#include "MyRTOS_Monitor.h"
#include "MyRTOS_Timer.h"
#include "MyRTOS_Shell.h"

void Platform_Console_HwInit(void);


void Platform_Console_OSInit(void);

void Platform_HiresTimer_Init(void);

void Platform_error_handler_init(void);

//系统标准流（将被注入）
extern StreamHandle_t g_system_stdin;
extern StreamHandle_t g_system_stdout;
extern StreamHandle_t g_system_stderr;

// 声明默认的Shell命令


#if (MYRTOS_SERVICE_SHELL_ENABLE == 1)
extern const ShellCommand_t g_default_shell_commands[];
extern const size_t g_default_shell_command_count;
ShellHandle_t g_platform_shell_handle = NULL;

void platform_register_default_commands(ShellHandle_t shell_h);
#endif

static void system_clock_config(void) {
    // (这里应放置标准系统时钟配置代码，例如从HSI/HSE启动PLL)
    //  除非默认时钟已由启动文件或SystemInit()正确配置
    // 如果没有，需要在这里添加RCU配置代码以达到 MYRTOS_CPU_CLOCK_HZ
    //复位RCU到默认状态
    rcu_deinit();
    //能外部高速晶振 (HXTAL)
    rcu_osci_on(RCU_HXTAL);
    //HXTAL 稳定
    if (SUCCESS != rcu_osci_stab_wait(RCU_HXTAL)) {
        // 如果HXTAL启动失败，可以在这里挂起或执行错误处理
        while (1);
    }
    //配置 AHB, APB1, APB2 总线的分频系数
    //    AHB  = SYSCLK / 1
    //    APB2 = AHB / 2
    //    APB1 = AHB / 4
    rcu_ahb_clock_config(RCU_AHB_CKSYS_DIV1);
    rcu_apb2_clock_config(RCU_APB2_CKAHB_DIV2);
    rcu_apb1_clock_config(RCU_APB1_CKAHB_DIV4);


    //配置主PLL参数
    //    公式: SYSCLK = (HXTAL / pll_m) * pll_n / pll_p
    //    对于168MHz系统时钟 (使用8MHz晶振): 168MHz = (8MHz / 8) * 336 / 2
    //    对于200MHz系统时钟 (使用8MHz晶振): 200MHz = (8MHz / 8) * 400 / 2
    uint32_t pll_m = 8;
    uint32_t pll_n = (MYRTOS_CPU_CLOCK_HZ / 1000000) * 2;
    uint32_t pll_p = 2; // 主分频因子 P = 2
    uint32_t pll_q = 7; // USB时钟分频因子 Q = 7 (例如, 336/7 = 48MHz for USB)
    // 如果 MYRTOS_CPU_CLOCK_HZ 是 200MHz, VCO 是 400MHz, Q应该设为8或更高
#if (MYRTOS_CPU_CLOCK_HZ == 200000000)
    pll_q = 9; // 400MHz / 9 approx 44.4MHz, good enough for RNG if USB not used.
#endif
    rcu_pll_config(RCU_PLLSRC_HXTAL, pll_m, pll_n, pll_p, pll_q);
    //使能主PLL
    rcu_osci_on(RCU_PLL_CK);
    //等待PLL稳定
    if (SUCCESS != rcu_osci_stab_wait(RCU_FLAG_PLLSTB)) {
        // 如果PLL锁定失败
        while (1);
    }
    //切换系统时钟源到主PLL
    rcu_system_clock_source_config(RCU_CKSYSSRC_PLLP);

    //等待切换完成
    while (RCU_SCSS_PLLP != rcu_system_clock_source_get()) {
        //不应发生哈
    }
}

void Platform_Init(void) {
    //核心硬件初始化
    // system_clock_config();
    nvic_priority_group_set(NVIC_PRIGROUP_PRE4_SUB0); // 设置中断优先级分组
    //调用早期钩子
    Platform_EarlyInit_Hook();
    //初始化平台驱动 (根据配置)
#if (PLATFORM_USE_CONSOLE == 1)
    Platform_Console_HwInit();
#endif
#if (PLATFORM_USE_HIRES_TIMER == 1)
    Platform_HiresTimer_Init();
#endif
    // 调用BSP初始化钩子
    Platform_BSP_Init_Hook();
    //初始化RTOS内核和服务 (根据RTOS配置)
    MyRTOS_Init();
    Platform_error_handler_init();
    Platform_Console_OSInit();
#if (MYRTOS_SERVICE_IO_ENABLE == 1)
    StdIOService_Init();
    // 注入平台实现的控制台流到RTOS IO服务
    StreamHandle_t console_stream = Platform_Console_GetStream();
    g_system_stdin = console_stream;
    g_system_stdout = console_stream;
    g_system_stderr = console_stream;
#endif

#if (MYRTOS_SERVICE_LOG_ENABLE == 1)
    Log_Init(LOG_LEVEL_INFO, g_system_stdout);
#endif

#if (MYRTOS_SERVICE_MONITOR_ENABLE == 1)
    MonitorConfig_t monitor_config = {.get_hires_timer_value = Platform_Timer_GetHiresValue};
    Monitor_Init(&monitor_config);
#endif

#if (MYRTOS_SERVICE_TIMER_ENABLE == 1)
    // 使用合理的默认值初始化定时器服务
    TimerService_Init(MYRTOS_MAX_PRIORITIES - 2, 2048);
#endif

    ShellHandle_t shell_h = NULL;
#if (MYRTOS_SERVICE_SHELL_ENABLE == 1)
    ShellConfig_t shell_config = {.io_stream = g_system_stdout, .prompt = "MyRTOS> "};
    g_platform_shell_handle = Shell_Init(&shell_config);
    if (g_platform_shell_handle) {
        shell_h = g_platform_shell_handle;
        platform_register_default_commands(g_platform_shell_handle);
    }
#endif

    //调用应用设置钩子
    Platform_AppSetup_Hook(shell_h);

#if (MYRTOS_SERVICE_SHELL_ENABLE == 1)
    if (g_platform_shell_handle) {
        Shell_Start(g_platform_shell_handle, "Shell", 4, 4096);
    }
#endif
}


void Platform_StartScheduler(void) {
    //让用户创建所有的应用程序任务
    Platform_CreateTasks_Hook();
    //启动RTOS调度器，并将平台提供的空闲任务钩子传入
    Task_StartScheduler(Platform_IdleTask_Hook);
    //下面的代码不应被执行
    while (1);
}
