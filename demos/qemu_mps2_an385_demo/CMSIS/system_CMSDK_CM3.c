/**
 * @brief System initialization for CMSDK Cortex-M3 (QEMU MPS2-AN385)
 */

#include "CMSDK_CM3.h"

// ============================================================================
//                           系统时钟
// ============================================================================

// MPS2-AN385 主时钟 25MHz
#define SYSTEM_CLOCK    25000000UL

uint32_t SystemCoreClock = SYSTEM_CLOCK;

/**
 * @brief 系统初始化
 */
void SystemInit(void) {
    // MPS2-AN385 不需要特殊的时钟配置
    SystemCoreClock = SYSTEM_CLOCK;
}

/**
 * @brief 更新系统时钟变量
 */
void SystemCoreClockUpdate(void) {
    SystemCoreClock = SYSTEM_CLOCK;
}
