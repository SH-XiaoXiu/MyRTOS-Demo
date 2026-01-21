//
// GD32F407 Demo Platform Hardware Initialization
//
// GD32F407 特定的硬件初始化：时钟树、NVIC
//

#include "platform.h"
#include "gd32f4xx.h"
#include "gd32f4xx_misc.h"
#include "gd32f4xx_rcu.h"

// ============================================================================
//                           时钟配置
// ============================================================================
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

// ============================================================================
//                           平台硬件初始化
// ============================================================================
void Platform_HwInit(void) {
    // 配置 NVIC 优先级分组
    nvic_priority_group_set(NVIC_PRIGROUP_PRE4_SUB0);

    // 配置系统时钟 (168MHz)
    system_clock_config();

    // 早期初始化钩子
    Platform_EarlyInit_Hook();

    // 初始化控制台硬件
#if (PLATFORM_USE_CONSOLE == 1)
    Platform_Console_HwInit();
#endif

    // 初始化高精度定时器
#if (PLATFORM_USE_HIRES_TIMER == 1)
    Platform_HiresTimer_Init();
#endif

    // BSP 初始化钩子
    Platform_BSP_Init_Hook();
    Platform_BSP_After_Hook();
}

// ============================================================================
//                           系统重启
// ============================================================================
void Platform_Reboot(void) {
    NVIC_SystemReset();
}
