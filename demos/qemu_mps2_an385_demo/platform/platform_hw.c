/**
 * @brief QEMU MPS2-AN385 平台硬件初始化
 */

#include "platform.h"
#include "CMSDK_CM3.h"

/**
 * @brief 平台硬件初始化
 */
void Platform_HwInit(void) {
    // 系统初始化
    SystemInit();

    // 早期初始化钩子
    Platform_EarlyInit_Hook();

    // 初始化控制台
#if PLATFORM_USE_CONSOLE == 1
    Platform_Console_HwInit();
#endif

    // 初始化高精度定时器
#if PLATFORM_USE_HIRES_TIMER == 1
    Platform_HiresTimer_Init();
#endif

    // BSP 初始化钩子
    Platform_BSP_Init_Hook();

    // BSP 初始化完成钩子
    Platform_BSP_After_Hook();
}
