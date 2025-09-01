//
// Created by XiaoXiu on 8/31/2025.
//

#include "platform.h"
#include "gd32f4xx.h"
#include "gd32f4xx_misc.h"
#include "gd32f4xx_rcu.h"

#include "MyRTOS.h"
#include "MyRTOS_IO.h"
#include "MyRTOS_Log.h"
#include "MyRTOS_Monitor.h"
#include "MyRTOS_Timer.h"
#include "MyRTOS_Shell.h"
#include "MyRTOS_VTS.h"

void Platform_Console_HwInit(void);

void Platform_Console_OSInit(void);

void Platform_HiresTimer_Init(void);

void Platform_error_handler_init(void);

static void system_clock_config(void);
#if (MYRTOS_SERVICE_SHELL_ENABLE == 1 && MYRTOS_SERVICE_VTS_ENABLE == 1)
void Platform_RegisterVTSCommands(ShellHandle_t shell_h, const VTS_Handles_t *handles);
#endif


// 全局变量
extern StreamHandle_t g_system_stdin;
extern StreamHandle_t g_system_stdout;
extern StreamHandle_t g_system_stderr;

#if (MYRTOS_SERVICE_SHELL_ENABLE == 1)
ShellHandle_t g_platform_shell_handle = NULL;

void platform_register_default_commands(ShellHandle_t shell_h);
#endif

#if (MYRTOS_SERVICE_SHELL_ENABLE == 1 && MYRTOS_SERVICE_VTS_ENABLE == 1)
// 用于保存Shell任务原始的stdout，以便在showall模式后恢复
static StreamHandle_t g_shell_original_stdout = NULL;
#endif


static void system_clock_config(void) {
    rcu_deinit();
    rcu_osci_on(RCU_HXTAL);
    // 等待HXTAL 稳定
    if (SUCCESS != rcu_osci_stab_wait(RCU_HXTAL)) {
        while (1);
    }
    // 配置 AHB, APB1, APB2 总线的分频系数
    rcu_ahb_clock_config(RCU_AHB_CKSYS_DIV1);
    rcu_apb2_clock_config(RCU_APB2_CKAHB_DIV2);
    rcu_apb1_clock_config(RCU_APB1_CKAHB_DIV4);

    // 配置主PLL参数: 168MHz = (8MHz / 8) * 336 / 2
    uint32_t pll_m = 8;
    uint32_t pll_n = 336; // for 168MHz
    uint32_t pll_p = 2;
    uint32_t pll_q = 7;
    rcu_pll_config(RCU_PLLSRC_HXTAL, pll_m, pll_n, pll_p, pll_q);
    // 使能主PLL
    rcu_osci_on(RCU_PLL_CK);
    // 等待PLL稳定
    if (SUCCESS != rcu_osci_stab_wait(RCU_FLAG_PLLSTB)) {
        while (1);
    }
    // 切换系统时钟源到主PLL
    rcu_system_clock_source_config(RCU_CKSYSSRC_PLLP);
    // 等待切换完成
    while (RCU_SCSS_PLLP != rcu_system_clock_source_get()) {
    }
}

void Platform_Init(void) {
    //核心硬件和早期钩子
    // system_clock_config();
    nvic_priority_group_set(NVIC_PRIGROUP_PRE4_SUB0);
    Platform_EarlyInit_Hook();

    //平台驱动初始化
#if (PLATFORM_USE_CONSOLE == 1)
    Platform_Console_HwInit();
#endif
#if (PLATFORM_USE_HIRES_TIMER == 1)
    Platform_HiresTimer_Init();
#endif
    Platform_BSP_Init_Hook();

    //RTOS内核和服务基础初始化
    MyRTOS_Init();
    Platform_error_handler_init();
#if (PLATFORM_USE_CONSOLE == 1)
    Platform_Console_OSInit();
#endif
#if (MYRTOS_SERVICE_IO_ENABLE == 1)
    StdIOService_Init();
#endif

    //根据配置决定IO流的连接方式
#if (MYRTOS_SERVICE_VTS_ENABLE == 1)
    //启用VTS，构建高级交互系统
    StreamHandle_t physical_stream = Platform_Console_GetStream();
    static VTS_Handles_t vts_handles; // 使用static使其在函数结束后依然有效

    if (VTS_Init(physical_stream, &vts_handles) != 0) {
        while (1); // VTS 初始化失败是致命错误
    }
    g_shell_original_stdout = vts_handles.primary_output_stream;
    // 设置系统默认标准输出为VTS的后台流
    g_system_stdout = vts_handles.background_stream;
    g_system_stderr = vts_handles.background_stream;

#if (MYRTOS_SERVICE_SHELL_ENABLE == 1)
    // 初始化Shell服务
    ShellConfig_t shell_config = {.prompt = "MyRTOS> "};
    g_platform_shell_handle = Shell_Init(&shell_config);
    if (g_platform_shell_handle) {
        TaskHandle_t shell_task_h = Shell_Start(g_platform_shell_handle, "Shell", 4, 4096);
        if (shell_task_h) {
            // 将Shell任务的IO重定向到VTS的 primary 通道
            Task_SetStdIn(shell_task_h, vts_handles.primary_input_stream);
            Task_SetStdOut(shell_task_h, vts_handles.primary_output_stream);
            Task_SetStdErr(shell_task_h, vts_handles.primary_output_stream);
            // 注册默认命令和VTS控制命令
            platform_register_default_commands(g_platform_shell_handle);
            Platform_RegisterVTSCommands(g_platform_shell_handle, &vts_handles);
        }
    }
#endif // MYRTOS_SERVICE_SHELL_ENABLE

#else
    //禁用VTS，基础直连模式
    StreamHandle_t physical_stream = Platform_Console_GetStream();
    g_system_stdin = physical_stream;
    g_system_stdout = physical_stream;
    g_system_stderr = physical_stream;

#if (MYRTOS_SERVICE_SHELL_ENABLE == 1)
    ShellConfig_t shell_config = {.prompt = "MyRTOS> "};
    g_platform_shell_handle = Shell_Init(&shell_config);
    if (g_platform_shell_handle) {
        // 获取创建的任务句柄
        TaskHandle_t shell_task_h = Shell_Start(g_platform_shell_handle, "Shell", 4, 4096);
        if (shell_task_h) {
            //显式地设置(或确认)其IO流 这样更好 我有洁癖
            Task_SetStdIn(shell_task_h, g_system_stdin);
            Task_SetStdOut(shell_task_h, g_system_stdout);
            Task_SetStdErr(shell_task_h, g_system_stderr);
            platform_register_default_commands(g_platform_shell_handle);
        }
    }
#endif // MYRTOS_SERVICE_SHELL_ENABLE

#endif // MYRTOS_SERVICE_VTS_ENABLE

    //初始化其他服务
#if (MYRTOS_SERVICE_LOG_ENABLE == 1)
    Log_Init(LOG_LEVEL_DEBUG, g_system_stdout);
#endif
#if (MYRTOS_SERVICE_MONITOR_ENABLE == 1)
    MonitorConfig_t monitor_config = {.get_hires_timer_value = Platform_Timer_GetHiresValue};
    Monitor_Init(&monitor_config);
#endif
#if (MYRTOS_SERVICE_TIMER_ENABLE == 1)
    TimerService_Init(MYRTOS_MAX_PRIORITIES - 2, 2048);
#endif

    //调用应用层设置钩子
    Platform_AppSetup_Hook(g_platform_shell_handle);
}

void Platform_StartScheduler(void) {
    // 让用户创建所有的应用程序任务
    Platform_CreateTasks_Hook();
    // 启动RTOS调度器
    Task_StartScheduler(Platform_IdleTask_Hook);

    while (1);
}

/**
 *VTS 控制命令的注册与实现 (仅当VTS启用时)
*/
#if (MYRTOS_SERVICE_SHELL_ENABLE == 1 && MYRTOS_SERVICE_VTS_ENABLE == 1)

// 命令处理函数的前置声明
int cmd_fg(ShellHandle_t shell_h, int argc, char *argv[]);

int cmd_bg(ShellHandle_t shell_h, int argc, char *argv[]);

int cmd_showall(ShellHandle_t shell_h, int argc, char *argv[]);

// 用于保存VTS句柄，以便Shell命令可以访问它们
static VTS_Handles_t g_cmd_vts_handles;

void Platform_RegisterVTSCommands(ShellHandle_t shell_h, const VTS_Handles_t *handles) {
    if (!shell_h || !handles) return;
    g_cmd_vts_handles = *handles;
    Shell_RegisterCommand(shell_h, "fg", "Focus on the background stream", cmd_fg);
    Shell_RegisterCommand(shell_h, "bg", "Focus back on the shell", cmd_bg);
    Shell_RegisterCommand(shell_h, "showall", "Show all streams mixed in background", cmd_showall);
}

// 命令实现
int cmd_fg(ShellHandle_t shell_h, int argc, char *argv[]) {
    (void) argc;
    (void) argv;

    // 恢复Shell的stdout到其原始流，以防之前在showall模式
    if (g_shell_original_stdout) {
        Task_SetStdOut(Task_GetCurrentTaskHandle(), g_shell_original_stdout);
    }

    MyRTOS_printf("Switching focus to background stream...\n");
    VTS_SetFocus(g_cmd_vts_handles.background_stream);
    return 0;
}

int cmd_bg(ShellHandle_t shell_h, int argc, char *argv[]) {
    (void) argc;
    (void) argv;

    // 恢复Shell的stdout到其原始流，以防之前在showall模式
    if (g_shell_original_stdout) {
        Task_SetStdOut(Task_GetCurrentTaskHandle(), g_shell_original_stdout);
    }

    // 先把焦点还给Shell，这样下面的提示信息才能被看到
    VTS_SetFocus(g_cmd_vts_handles.primary_output_stream);
    MyRTOS_printf("Switching focus back to shell...\n");
    return 0;
}

int cmd_showall(ShellHandle_t shell_h, int argc, char *argv[]) {
    (void) argc;
    (void) argv;
    MyRTOS_printf("Mixing all outputs into background stream. Use 'bg' to restore.\n");

    //将Shell自己的输出也重定向到后台流
    Task_SetStdOut(Task_GetCurrentTaskHandle(), g_cmd_vts_handles.background_stream);

    //将焦点设置到后台流
    VTS_SetFocus(g_cmd_vts_handles.background_stream);
    return 0;
}
#endif // (MYRTOS_SERVICE_SHELL_ENABLE && MYRTOS_SERVICE_VTS_ENABLE)
