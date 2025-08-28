//
// Created by XiaoXiu on 8/29/2025.
//
// Platform: ARM Cortex-M4F on GD32F4xx
//

#include <string.h>

#include "MyRTOS.h"
#include "MyRTOS_Monitor.h" // For HardFault Handler logging
#include "MyRTOS_Port.h"

// 外部全局变量，从 MyRTOS.c 引用
extern TaskHandle_t currentTask;

extern void *schedule_next_task(void);

// 外部函数，从 MyRTOS.c 引用，用于处理Tick逻辑
extern void MyRTOS_Tick_Handler(void);


/**
 * @brief 初始化任务的栈帧
 */
StackType_t *MyRTOS_Port_InitialiseStack(StackType_t *pxTopOfStack, void (*pxCode)(void *), void *pvParameters) {
    // 偏移栈顶指针，确保8字节对齐
    pxTopOfStack = (StackType_t *) (((uint32_t) pxTopOfStack) & ~0x07UL);

    pxTopOfStack--;
    *pxTopOfStack = 0x01000000; // xPSR
    pxTopOfStack--;
    *pxTopOfStack = ((uint32_t) pxCode) | 1u; // PC
    pxTopOfStack--;
    *pxTopOfStack = 0; // LR
    pxTopOfStack--;
    *pxTopOfStack = 0x12121212; // R12
    pxTopOfStack--;
    *pxTopOfStack = 0x03030303; // R3
    pxTopOfStack--;
    *pxTopOfStack = 0x02020202; // R2
    pxTopOfStack--;
    *pxTopOfStack = 0x01010101; // R1
    pxTopOfStack--;
    *pxTopOfStack = (uint32_t) pvParameters; // R0
    pxTopOfStack--;
    *pxTopOfStack = 0x0B0B0B0B; // R11
    pxTopOfStack--;
    *pxTopOfStack = 0x0A0A0A0A; // R10
    pxTopOfStack--;
    *pxTopOfStack = 0x09090909; // R9
    pxTopOfStack--;
    *pxTopOfStack = 0x08080808; // R8
    pxTopOfStack--;
    *pxTopOfStack = 0x07070707; // R7
    pxTopOfStack--;
    *pxTopOfStack = 0x06060606; // R6
    pxTopOfStack--;
    *pxTopOfStack = 0x05050505; // R5
    pxTopOfStack--;
    *pxTopOfStack = 0x04040404; // R4
    pxTopOfStack--;
    *pxTopOfStack = 0xFFFFFFFD; // EXC_RETURN

    return pxTopOfStack;
}

/**
 * @brief 启动RTOS调度器
 */
BaseType_t MyRTOS_Port_StartScheduler(void) {
    // 配置 PendSV 和 SysTick 的优先级
    SCB->VTOR = (uint32_t) 0x08000000;
    NVIC_SetPriority(PendSV_IRQn, 0xFF);
    NVIC_SetPriority(SysTick_IRQn, 0x00);

    // 启动 SysTick 定时器
    if (SysTick_Config(SystemCoreClock / MY_RTOS_TICK_RATE_HZ)) {
        while (1);
    }

    // 第一次调度,手动选择下一个任务
    schedule_next_task();

    __asm volatile(
        "ldr r0, =0xE000ED08\n"
        "ldr r0, [r0]\n"
        "ldr r0, [r0]\n"
        "msr msp, r0\n"
    );

    // 触发SVC中断，启动第一个任务
    __asm volatile("svc 0");

    return -1;
}

/**
 * @brief SVC Handler
 */
__attribute__((naked)) void SVC_Handler(void) {
    __asm volatile (
        " ldr r0, =currentTask      \n"
        " ldr r0, [r0]              \n"
        " ldr r0, [r0]              \n"

        " ldmia r0!, {r1, r4-r11}   \n"
        " mov lr, r1                \n"

        " tst lr, #0x10             \n"
        " it eq                     \n"
        " vldmiaeq r0!, {s16-s31}   \n"

        " msr psp, r0               \n"
        " isb                       \n"

        " movs r0, #2               \n"
        " msr control, r0           \n"
        " isb                       \n"

        " bx lr                     \n"
    );
}

/**
 * @brief PendSV Handler
 */
__attribute__((naked)) void PendSV_Handler(void) {
    __asm volatile (
        " mrs r0, psp                       \n"
        " isb                               \n"

        " ldr r2, =currentTask              \n"
        " ldr r3, [r2]                      \n"
        " cbz r3, 1f                        \n"

        " tst lr, #0x10                     \n"
        " it eq                             \n"
        " vstmdbeq r0!, {s16-s31}           \n"

        " mov r1, lr                        \n"
        " stmdb r0!, {r1, r4-r11}           \n"

        " str r0, [r3]                      \n"

        "1:                                 \n"
        " cpsid i                           \n"
        " bl schedule_next_task             \n"
        " cpsie i                           \n"

        " ldr r2, =currentTask              \n"
        " ldr r2, [r2]                      \n"
        " ldr r0, [r2]                      \n"

        " ldmia r0!, {r1, r4-r11}           \n"
        " mov lr, r1                        \n"

        " tst lr, #0x10                     \n"
        " it eq                             \n"
        " vldmiaeq r0!, {s16-s31}           \n"

        " msr psp, r0                       \n"
        " isb                               \n"

        " bx lr                             \n"
    );
}

/**
 * @brief SysTick Handler
 */
void SysTick_Handler(void) {
    uint32_t primask_status;
    MyRTOS_Port_ENTER_CRITICAL(primask_status);

    MyRTOS_Tick_Handler();

    MyRTOS_Port_EXIT_CRITICAL(primask_status);

    MyRTOS_Port_YIELD();
}

/**
 * @brief HardFault Handler
 */
void HardFault_Handler(void) {
    __disable_irq();

#ifndef SCB_CFSR_IACCVIOL_Msk
#define SCB_CFSR_IACCVIOL_Msk     (1UL << 0)
#define SCB_CFSR_DACCVIOL_Msk     (1UL << 1)
#define SCB_CFSR_MUNSTKERR_Msk    (1UL << 3)
#define SCB_CFSR_MSTKERR_Msk      (1UL << 4)
#define SCB_CFSR_MLSPERR_Msk      (1UL << 5)
#define SCB_CFSR_IBUSERR_Msk      (1UL << 8)
#define SCB_CFSR_PRECISERR_Msk    (1UL << 9)
#define SCB_CFSR_IMPRECISERR_Msk  (1UL << 10)
#define SCB_CFSR_UNSTKERR_Msk     (1UL << 11)
#define SCB_CFSR_STKERR_Msk       (1UL << 12)
#define SCB_CFSR_LSPERR_Msk       (1UL << 13)
#define SCB_CFSR_UNDEFINSTR_Msk   (1UL << 16)
#define SCB_CFSR_INVSTATE_Msk     (1UL << 17)
#define SCB_CFSR_INVPC_Msk        (1UL << 18)
#endif

    uint32_t stacked_pc = 0;
    uint32_t sp = 0;
    uint32_t cfsr = SCB->CFSR;
    uint32_t hfsr = SCB->HFSR;
    uint32_t bfar = SCB->BFAR;

    register uint32_t lr __asm("lr");

    if (lr & 0x4) {
        sp = __get_PSP();
    } else {
        sp = __get_MSP();
    }

    stacked_pc = ((uint32_t *) sp)[6];

    MY_RTOS_KERNEL_LOGE("\n!!! Hard Fault !!!.\n.\n");
    MY_RTOS_KERNEL_LOGE("CFSR: 0x%08lX, HFSR: 0x%08lX", cfsr, hfsr);

    if (cfsr & (1UL << 15)) {
        MY_RTOS_KERNEL_LOGE("Bus Fault Address: 0x%08lX", bfar);
    }

    MY_RTOS_KERNEL_LOGE("LR: 0x%08lX, SP: 0x%08lX, Stacked PC: 0x%08lX", lr, sp, stacked_pc);

    if (cfsr & SCB_CFSR_IACCVIOL_Msk)
        MY_RTOS_KERNEL_LOGE("Fault: Instruction Access Violation.\n.\n");
    if (cfsr & SCB_CFSR_DACCVIOL_Msk)
        MY_RTOS_KERNEL_LOGE("Fault: Data Access Violation.\n.\n");
    if (cfsr & SCB_CFSR_UNDEFINSTR_Msk)
        MY_RTOS_KERNEL_LOGE("Fault: Undefined Instruction.\n.\n");

    while (1);
}
