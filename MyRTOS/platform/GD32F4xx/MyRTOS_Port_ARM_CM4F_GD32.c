//
// Created by XiaoXiu on 8/29/2025.
// Platform: ARM Cortex-M4F on GD32F4xx
// This is a corrected and adapted version of the original port file.
//

#include <string.h>
#include <stdio.h>

#include "MyRTOS.h"
#include "MyRTOS_Port.h"
#include "MyRTOS_Platform.h"

//从 MyRTOS.c 引用
extern TaskHandle_t currentTask;

// 外部函数
extern void *schedule_next_task(void);


static void enableFPU(void);

void MyRTOS_Idle_Task(void *pv) {
    while (1) {
        __WFI();
    }
}

/**
 * @brief 初始化任务的栈帧
 */
StackType_t *MyRTOS_Port_InitialiseStack(StackType_t *pxTopOfStack, void (*pxCode)(void *), void *pvParameters) {
    // 偏移栈顶指针，确保8字节对齐
    pxTopOfStack = (StackType_t *) (((uint32_t) pxTopOfStack) & ~0x07UL);

    // 硬件自动保存的帧 (R0-R3, R12, LR, PC, xPSR)
    pxTopOfStack--;
    *pxTopOfStack = 0x01000000; // xPSR (Thumb state)
    pxTopOfStack--;
    *pxTopOfStack = ((uint32_t) pxCode) | 1u; // PC (with Thumb bit)
    pxTopOfStack--;
    *pxTopOfStack = 0; // LR (任务不应返回)
    pxTopOfStack--;
    *pxTopOfStack = 0x12121212; // R12 (任意值)
    pxTopOfStack--;
    *pxTopOfStack = 0x03030303; // R3 (任意值)
    pxTopOfStack--;
    *pxTopOfStack = 0x02020202; // R2 (任意值)
    pxTopOfStack--;
    *pxTopOfStack = 0x01010101; // R1 (任意值)
    pxTopOfStack--;
    *pxTopOfStack = (uint32_t) pvParameters; // R0 (任务参数)

    // 软件手动保存的帧 (R4-R11, EXC_RETURN)
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
    // EXC_RETURN: 返回线程模式, 使用PSP。
    // 第4位为1表示栈上没有FPU内容，为0表示有。
    // 初始时假定没有，FPU上下文将懒加载。
    *pxTopOfStack = 0xFFFFFFFD;

    return pxTopOfStack;
}


/**
 * @brief 启用FPU
 */
static void enableFPU(void) {
    SCB->CPACR |= ((3UL << 10 * 2) | (3UL << 11 * 2));
    __ISB();
    __DSB();
}

/**
 * @brief 启动RTOS调度器
 */
BaseType_t MyRTOS_Port_StartScheduler(void) {
    // 启用 FPU
    enableFPU();

    // 配置 PendSV 和 SysTick 的优先级
    NVIC_SetPriority(PendSV_IRQn, 255);
    NVIC_SetPriority(SysTick_IRQn, 15);

    // 启动 SysTick 定时器
    if (SysTick_Config(SystemCoreClock / MY_RTOS_TICK_RATE_HZ)) {
        while (1);
    }

    // 第一次调度,手动选择下一个任务
    schedule_next_task();

    // 触发SVC中断，启动第一个任务
    __asm volatile("svc 0");

    return -1;
}

/**
 * @brief SVC Handler (适配原始栈帧模型和FPU)
 */
__attribute__((naked)) void SVC_Handler(void) {
    __asm volatile (
        " ldr r0, =currentTask      \n"
        " ldr r0, [r0]              \n"
        " ldr r0, [r0]              \n" // r0 = 任务的栈指针(sp)

        " ldmia r0!, {r1, r4-r11}   \n" // 从栈上恢复 EXC_RETURN (到r1) 和 R4-R11
        " mov lr, r1                \n" // 将EXC_RETURN值移入LR寄存器

        " msr psp, r0               \n" // 更新PSP，使其指向硬件保存的栈帧
        " isb                       \n"

        " movs r0, #2               \n" // 设置CONTROL寄存器，切换到PSP
        " msr control, r0           \n"
        " isb                       \n"

        " bx lr                     \n" // 异常返回，硬件将自动从PSP恢复剩余寄存器
    );
}

/**
 * @brief PendSV Handler (适配原始栈帧模型和FPU)
 */
__attribute__((naked)) void PendSV_Handler(void) {
    __asm volatile (
        " mrs r0, psp                       \n" // 获取当前任务的PSP
        " isb                               \n"

        " ldr r2, =currentTask              \n"
        " ldr r3, [r2]                      \n" // r3 = 当前任务的TCB

        " tst lr, #0x10                     \n" // 检查LR[4]，判断是否使用了FPU
        " it eq                             \n"
        " vstmdbeq r0!, {s16-s31}           \n" // 如果使用了，保存FPU寄存器

        " mov r1, lr                        \n" // 将LR(EXC_RETURN)保存到r1
        " stmdb r0!, {r1, r4-r11}           \n" // 保存软件上下文

        " str r0, [r3]                      \n" // 将新的SP保存回TCB

        " bl schedule_next_task             \n" // 选择下一个任务

        " ldr r2, =currentTask              \n"
        " ldr r2, [r2]                      \n" // r2 = 新任务的TCB
        " ldr r0, [r2]                      \n" // r0 = 新任务的SP

        " ldmia r0!, {r1, r4-r11}           \n" // 恢复软件上下文
        " mov lr, r1                        \n" // 恢复EXC_RETURN到LR

        " tst lr, #0x10                     \n" // 检查新任务是否使用了FPU
        " it eq                             \n"
        " vldmiaeq r0!, {s16-s31}           \n" // 如果是，恢复FPU寄存器

        " msr psp, r0                       \n" // 更新PSP
        " isb                               \n"
        " bx lr                             \n" // 异常返回
    );
}

/**
 * @brief SysTick Handler
 */
void SysTick_Handler(void) {
    MyRTOS_Port_ENTER_CRITICAL();
    MyRTOS_Tick_Handler();
    MyRTOS_Port_EXIT_CRITICAL();
    MyRTOS_Port_YIELD();
}


static void prvHardFaultPrint(const char *str) {
    for (const char *p = str; *p; ++p) { MyRTOS_Platform_PutChar(*p); }
}

/**
 * @brief HardFault Handler (适配新API)
 */
void HardFault_Handler(void) {
    __disable_irq();

    char fault_buffer[128];
    char task_name_buffer[MY_RTOS_TASK_NAME_MAX_LEN];
    uint32_t task_id;
    uint32_t stacked_pc;
    uint32_t sp = __get_PSP();
    uint32_t cfsr = SCB->CFSR;
    uint32_t hfsr = SCB->HFSR;
    uint32_t bfar = SCB->BFAR;

    stacked_pc = ((uint32_t *) sp)[6];

    prvHardFaultPrint("\r\n\r\n!!! Hard Fault !!!\r\n");
    snprintf(fault_buffer, sizeof(fault_buffer), "  Stacked PC: 0x%08lX\r\n", stacked_pc);
    prvHardFaultPrint(fault_buffer);
    snprintf(fault_buffer, sizeof(fault_buffer), "  CFSR: 0x%08lX, HFSR: 0x%08lX\r\n", cfsr, hfsr);
    prvHardFaultPrint(fault_buffer);

    if (cfsr & (1UL << 15)) {
        prvHardFaultPrint("  Bus Fault occurred.\r\n");
        snprintf(fault_buffer, sizeof(fault_buffer), "  Bus Fault Address: 0x%08lX\r\n", bfar);
        prvHardFaultPrint(fault_buffer);
    }

    if (Task_GetDebugInfo(task_name_buffer, sizeof(task_name_buffer), &task_id)) {
        snprintf(fault_buffer, sizeof(fault_buffer), "  Fault in Task: %s (ID: %lu)\r\n",
                 task_name_buffer, (unsigned long) task_id);
        prvHardFaultPrint(fault_buffer);
    } else {
        prvHardFaultPrint("  Fault occurred before scheduler started or in ISR.\r\n");
    }

    while (1);
}
