/**
 * @brief QEMU MPS2-AN385 高精度定时器驱动 (TIMER0)
 */

#include "platform.h"
#include "CMSDK_CM3.h"

#if PLATFORM_USE_HIRES_TIMER == 1

void Platform_HiresTimer_Init(void) {
    // 配置 TIMER0 为自由运行的向上计数器
    CMSDK_TIMER0->CTRL = 0;                     // 先停止
    CMSDK_TIMER0->VALUE = 0;                    // 清零
    CMSDK_TIMER0->RELOAD = 0xFFFFFFFF;          // 最大重载值
    CMSDK_TIMER0->CTRL = CMSDK_TIMER_CTRL_EN_Msk;  // 启动
}

uint32_t Platform_Timer_GetHiresValue(void) {
    return CMSDK_TIMER0->VALUE;
}

#endif /* PLATFORM_USE_HIRES_TIMER */
