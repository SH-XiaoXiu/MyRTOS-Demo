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

//ϵͳ��׼��������ע�룩
extern StreamHandle_t g_system_stdin;
extern StreamHandle_t g_system_stdout;
extern StreamHandle_t g_system_stderr;

// ����Ĭ�ϵ�Shell����


#if (MYRTOS_SERVICE_SHELL_ENABLE == 1)
extern const ShellCommand_t g_default_shell_commands[];
extern const size_t g_default_shell_command_count;
ShellHandle_t g_platform_shell_handle = NULL;

void platform_register_default_commands(ShellHandle_t shell_h);
#endif

static void system_clock_config(void) {
    // (����Ӧ���ñ�׼ϵͳʱ�����ô��룬�����HSI/HSE����PLL)
    //  ����Ĭ��ʱ�����������ļ���SystemInit()��ȷ����
    // ���û�У���Ҫ���������RCU���ô����Դﵽ MYRTOS_CPU_CLOCK_HZ
    //��λRCU��Ĭ��״̬
    rcu_deinit();
    //���ⲿ���پ��� (HXTAL)
    rcu_osci_on(RCU_HXTAL);
    //HXTAL �ȶ�
    if (SUCCESS != rcu_osci_stab_wait(RCU_HXTAL)) {
        // ���HXTAL����ʧ�ܣ���������������ִ�д�����
        while (1);
    }
    //���� AHB, APB1, APB2 ���ߵķ�Ƶϵ��
    //    AHB  = SYSCLK / 1
    //    APB2 = AHB / 2
    //    APB1 = AHB / 4
    rcu_ahb_clock_config(RCU_AHB_CKSYS_DIV1);
    rcu_apb2_clock_config(RCU_APB2_CKAHB_DIV2);
    rcu_apb1_clock_config(RCU_APB1_CKAHB_DIV4);


    //������PLL����
    //    ��ʽ: SYSCLK = (HXTAL / pll_m) * pll_n / pll_p
    //    ����168MHzϵͳʱ�� (ʹ��8MHz����): 168MHz = (8MHz / 8) * 336 / 2
    //    ����200MHzϵͳʱ�� (ʹ��8MHz����): 200MHz = (8MHz / 8) * 400 / 2
    uint32_t pll_m = 8;
    uint32_t pll_n = (MYRTOS_CPU_CLOCK_HZ / 1000000) * 2;
    uint32_t pll_p = 2; // ����Ƶ���� P = 2
    uint32_t pll_q = 7; // USBʱ�ӷ�Ƶ���� Q = 7 (����, 336/7 = 48MHz for USB)
    // ��� MYRTOS_CPU_CLOCK_HZ �� 200MHz, VCO �� 400MHz, QӦ����Ϊ8�����
#if (MYRTOS_CPU_CLOCK_HZ == 200000000)
    pll_q = 9; // 400MHz / 9 approx 44.4MHz, good enough for RNG if USB not used.
#endif
    rcu_pll_config(RCU_PLLSRC_HXTAL, pll_m, pll_n, pll_p, pll_q);
    //ʹ����PLL
    rcu_osci_on(RCU_PLL_CK);
    //�ȴ�PLL�ȶ�
    if (SUCCESS != rcu_osci_stab_wait(RCU_FLAG_PLLSTB)) {
        // ���PLL����ʧ��
        while (1);
    }
    //�л�ϵͳʱ��Դ����PLL
    rcu_system_clock_source_config(RCU_CKSYSSRC_PLLP);

    //�ȴ��л����
    while (RCU_SCSS_PLLP != rcu_system_clock_source_get()) {
        //��Ӧ������
    }
}

void Platform_Init(void) {
    //����Ӳ����ʼ��
    // system_clock_config();
    nvic_priority_group_set(NVIC_PRIGROUP_PRE4_SUB0); // �����ж����ȼ�����
    //�������ڹ���
    Platform_EarlyInit_Hook();
    //��ʼ��ƽ̨���� (��������)
#if (PLATFORM_USE_CONSOLE == 1)
    Platform_Console_HwInit();
#endif
#if (PLATFORM_USE_HIRES_TIMER == 1)
    Platform_HiresTimer_Init();
#endif
    // ����BSP��ʼ������
    Platform_BSP_Init_Hook();
    //��ʼ��RTOS�ں˺ͷ��� (����RTOS����)
    MyRTOS_Init();
    Platform_error_handler_init();
    Platform_Console_OSInit();
#if (MYRTOS_SERVICE_IO_ENABLE == 1)
    StdIOService_Init();
    // ע��ƽ̨ʵ�ֵĿ���̨����RTOS IO����
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
    // ʹ�ú����Ĭ��ֵ��ʼ����ʱ������
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

    //����Ӧ�����ù���
    Platform_AppSetup_Hook(shell_h);

#if (MYRTOS_SERVICE_SHELL_ENABLE == 1)
    if (g_platform_shell_handle) {
        Shell_Start(g_platform_shell_handle, "Shell", 4, 4096);
    }
#endif
}


void Platform_StartScheduler(void) {
    //���û��������е�Ӧ�ó�������
    Platform_CreateTasks_Hook();
    //����RTOS������������ƽ̨�ṩ�Ŀ��������Ӵ���
    Task_StartScheduler(Platform_IdleTask_Hook);
    //����Ĵ��벻Ӧ��ִ��
    while (1);
}
