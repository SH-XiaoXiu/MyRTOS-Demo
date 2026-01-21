/**
 * @brief CMSIS Device Header for ARM Cortex-M3 (QEMU MPS2-AN385)
 *
 * 此文件为 QEMU mps2-an385 模拟板提供设备定义
 */

#ifndef CMSDK_CM3_H
#define CMSDK_CM3_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
//                           中断号定义
// ============================================================================

typedef enum IRQn {
    /* Cortex-M3 处理器异常 */
    NonMaskableInt_IRQn     = -14,
    HardFault_IRQn          = -13,
    MemoryManagement_IRQn   = -12,
    BusFault_IRQn           = -11,
    UsageFault_IRQn         = -10,
    SVCall_IRQn             = -5,
    DebugMonitor_IRQn       = -4,
    PendSV_IRQn             = -2,
    SysTick_IRQn            = -1,

    /* MPS2-AN385 外设中断 */
    UART0_IRQn              = 0,
    UART1_IRQn              = 1,
    UART2_IRQn              = 2,
    TIMER0_IRQn             = 8,
    TIMER1_IRQn             = 9,
    DUALTIMER_IRQn          = 10,
} IRQn_Type;

// ============================================================================
//                           处理器和核心外设配置
// ============================================================================

#define __CM3_REV               0x0201      /* Cortex-M3 revision */
#define __MPU_PRESENT           1           /* MPU present */
#define __NVIC_PRIO_BITS        3           /* 8 优先级位 */
#define __Vendor_SysTickConfig  0           /* 使用标准 SysTick */

// 包含 CMSIS Cortex-M3 核心头文件
#include "core_cm3.h"

// ============================================================================
//                           系统时钟
// ============================================================================

extern uint32_t SystemCoreClock;

void SystemInit(void);
void SystemCoreClockUpdate(void);

// ============================================================================
//                           外设寄存器定义
// ============================================================================

/* UART */
typedef struct {
    __IO uint32_t DATA;         /* 0x00 数据寄存器 */
    __IO uint32_t STATE;        /* 0x04 状态寄存器 */
    __IO uint32_t CTRL;         /* 0x08 控制寄存器 */
    __IO uint32_t INTSTATUS;    /* 0x0C 中断状态 */
    __IO uint32_t BAUDDIV;      /* 0x10 波特率分频 */
} CMSDK_UART_TypeDef;

/* UART 状态位 */
#define CMSDK_UART_STATE_TXFULL_Pos     0
#define CMSDK_UART_STATE_TXFULL_Msk     (1UL << CMSDK_UART_STATE_TXFULL_Pos)
#define CMSDK_UART_STATE_RXFULL_Pos     1
#define CMSDK_UART_STATE_RXFULL_Msk     (1UL << CMSDK_UART_STATE_RXFULL_Pos)

/* UART 控制位 */
#define CMSDK_UART_CTRL_TXEN_Pos        0
#define CMSDK_UART_CTRL_TXEN_Msk        (1UL << CMSDK_UART_CTRL_TXEN_Pos)
#define CMSDK_UART_CTRL_RXEN_Pos        1
#define CMSDK_UART_CTRL_RXEN_Msk        (1UL << CMSDK_UART_CTRL_RXEN_Pos)
#define CMSDK_UART_CTRL_TXIRQEN_Pos     2
#define CMSDK_UART_CTRL_TXIRQEN_Msk     (1UL << CMSDK_UART_CTRL_TXIRQEN_Pos)
#define CMSDK_UART_CTRL_RXIRQEN_Pos     3
#define CMSDK_UART_CTRL_RXIRQEN_Msk     (1UL << CMSDK_UART_CTRL_RXIRQEN_Pos)

/* UART 中断状态位 */
#define CMSDK_UART_INTSTATUS_TX_Pos     0
#define CMSDK_UART_INTSTATUS_TX_Msk     (1UL << CMSDK_UART_INTSTATUS_TX_Pos)
#define CMSDK_UART_INTSTATUS_RX_Pos     1
#define CMSDK_UART_INTSTATUS_RX_Msk     (1UL << CMSDK_UART_INTSTATUS_RX_Pos)

/* Timer */
typedef struct {
    __IO uint32_t CTRL;         /* 0x00 控制寄存器 */
    __IO uint32_t VALUE;        /* 0x04 当前值 */
    __IO uint32_t RELOAD;       /* 0x08 重载值 */
    __IO uint32_t INTSTATUS;    /* 0x0C 中断状态/清除 */
} CMSDK_TIMER_TypeDef;

/* Timer 控制位 */
#define CMSDK_TIMER_CTRL_EN_Pos         0
#define CMSDK_TIMER_CTRL_EN_Msk         (1UL << CMSDK_TIMER_CTRL_EN_Pos)
#define CMSDK_TIMER_CTRL_SELEXTEN_Pos   1
#define CMSDK_TIMER_CTRL_SELEXTEN_Msk   (1UL << CMSDK_TIMER_CTRL_SELEXTEN_Pos)
#define CMSDK_TIMER_CTRL_SELEXTCLK_Pos  2
#define CMSDK_TIMER_CTRL_SELEXTCLK_Msk  (1UL << CMSDK_TIMER_CTRL_SELEXTCLK_Pos)
#define CMSDK_TIMER_CTRL_IRQEN_Pos      3
#define CMSDK_TIMER_CTRL_IRQEN_Msk      (1UL << CMSDK_TIMER_CTRL_IRQEN_Pos)

// ============================================================================
//                           外设基地址
// ============================================================================

#define CMSDK_UART0_BASE        (0x40004000UL)
#define CMSDK_UART1_BASE        (0x40005000UL)
#define CMSDK_UART2_BASE        (0x40006000UL)
#define CMSDK_TIMER0_BASE       (0x40000000UL)
#define CMSDK_TIMER1_BASE       (0x40001000UL)

// ============================================================================
//                           外设实例
// ============================================================================

#define CMSDK_UART0             ((CMSDK_UART_TypeDef *) CMSDK_UART0_BASE)
#define CMSDK_UART1             ((CMSDK_UART_TypeDef *) CMSDK_UART1_BASE)
#define CMSDK_UART2             ((CMSDK_UART_TypeDef *) CMSDK_UART2_BASE)
#define CMSDK_TIMER0            ((CMSDK_TIMER_TypeDef *) CMSDK_TIMER0_BASE)
#define CMSDK_TIMER1            ((CMSDK_TIMER_TypeDef *) CMSDK_TIMER1_BASE)

#ifdef __cplusplus
}
#endif

#endif /* CMSDK_CM3_H */
