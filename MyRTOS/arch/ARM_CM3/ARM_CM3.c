//
// MyRTOS Architecture Layer - ARM Cortex-M3
//
// 此文件实现了 ARM Cortex-M3 架构的移植层
// 包含：上下文切换、中断处理、临界区管理
//
// 适用于所有 Cortex-M3 芯片及 QEMU mps2-an385
//

#include "MyRTOS.h"
#include "MyRTOS_Kernel_Private.h"
#include "MyRTOS_Port.h"

extern TaskHandle_t currentTask;

// ============================================================================
//                           平台钩子函数声明
// ============================================================================
// 这些函数由用户在 platform 层实现
extern void Platform_HardFault_Hook(uint32_t *pulFaultStackAddress);
extern void Platform_StackOverflow_Hook(TaskHandle_t pxTask);

// ============================================================================
//                              临界区管理
// ============================================================================
volatile UBaseType_t uxCriticalNesting = 0;

void MyRTOS_Port_EnterCritical(void) {
    __disable_irq();
    uxCriticalNesting++;
}

void MyRTOS_Port_ExitCritical(void) {
    if (uxCriticalNesting > 0) {
        uxCriticalNesting--;
        if (uxCriticalNesting == 0) {
            __enable_irq();
        }
    }
}

// ============================================================================
//                              上下文切换
// ============================================================================
void MyRTOS_Port_Yield(void) {
    SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
    __ISB();
    __DSB();
}

void MyRTOS_Port_YieldFromISR(BaseType_t higherPriorityTaskWoken) {
    if (higherPriorityTaskWoken != 0) {
        MyRTOS_Port_Yield();
    }
}

// ============================================================================
//                              栈初始化
// ============================================================================
StackType_t *MyRTOS_Port_InitialiseStack(StackType_t *pxTopOfStack, void (*pxCode)(void *), void *pvParameters) {
    pxTopOfStack = (StackType_t *) (((UBaseType_t) pxTopOfStack) & ~((UBaseType_t) 7));

    // 硬件自动保存的栈帧 (异常返回时由硬件恢复)
    pxTopOfStack--;
    *pxTopOfStack = 0x01000000UL; // xPSR (Thumb位必须为1)
    pxTopOfStack--;
    *pxTopOfStack = ((UBaseType_t) pxCode) & (~(UBaseType_t) 1); // PC
    pxTopOfStack--;
    *pxTopOfStack = 0; // LR
    pxTopOfStack--;
    *pxTopOfStack = 0x12121212UL; // R12
    pxTopOfStack--;
    *pxTopOfStack = 0x03030303UL; // R3
    pxTopOfStack--;
    *pxTopOfStack = 0x02020202UL; // R2
    pxTopOfStack--;
    *pxTopOfStack = 0x01010101UL; // R1
    pxTopOfStack--;
    *pxTopOfStack = (UBaseType_t) pvParameters; // R0

    // 软件保存的栈帧 (由PendSV保存/恢复)
    pxTopOfStack--;
    *pxTopOfStack = 0x11111111UL; // R11
    pxTopOfStack--;
    *pxTopOfStack = 0x10101010UL; // R10
    pxTopOfStack--;
    *pxTopOfStack = 0x09090909UL; // R9
    pxTopOfStack--;
    *pxTopOfStack = 0x08080808UL; // R8
    pxTopOfStack--;
    *pxTopOfStack = 0x07070707UL; // R7
    pxTopOfStack--;
    *pxTopOfStack = 0x06060606UL; // R6
    pxTopOfStack--;
    *pxTopOfStack = 0x05050505UL; // R5
    pxTopOfStack--;
    *pxTopOfStack = 0x04040404UL; // R4
    pxTopOfStack--;
    *pxTopOfStack = 0xFFFFFFFDUL; // EXC_RETURN (返回线程模式，使用PSP)

    return pxTopOfStack;
}

// ============================================================================
//                           启动调度器
// ============================================================================
BaseType_t MyRTOS_Port_StartScheduler(void) {
    NVIC_SetPriority(PendSV_IRQn, (1 << __NVIC_PRIO_BITS) - 1);
    NVIC_SetPriority(SysTick_IRQn, (1 << __NVIC_PRIO_BITS) - 1);
    if (SysTick_Config(SystemCoreClock / MYRTOS_TICK_RATE_HZ) != 0) {
        return 1;
    }
    __asm volatile("svc 0" ::: "memory");
    return 0;
}

// ============================================================================
//                           异常处理器
// ============================================================================
void SVC_Handler(void) __attribute__((naked));
void SVC_Handler(void) {
    __asm volatile(
        " ldr r0, =currentTask      \n" // r0 = &currentTask
        " ldr r0, [r0]              \n" // r0 = currentTask (TCB指针)
        " ldr r0, [r0]              \n" // r0 = currentTask->sp

        " ldmia r0!, {r1, r4-r11}   \n" // 恢复 EXC_RETURN(r1) 和 R4-R11
        " mov lr, r1                \n" // LR = EXC_RETURN

        " msr psp, r0               \n" // PSP = r0 (指向硬件栈帧)
        " isb                       \n"

        " movs r0, #2               \n" // CONTROL = 2 (使用PSP)
        " msr control, r0           \n"
        " isb                       \n"

        " bx lr                     \n" // 异常返回
    );
}

void PendSV_Handler(void) __attribute__((naked));
void PendSV_Handler(void) {
    __asm volatile(
        " mrs r0, psp                       \n" // r0 = PSP
        " isb                               \n"

        " ldr r2, =currentTask              \n"
        " ldr r3, [r2]                      \n" // r3 = 当前TCB指针

        // 栈溢出检查
        " ldr r1, [r3, %0]                  \n" // r1 = tcb->stack_base
        " cmp r0, r1                        \n"
        " bge .L_no_overflow_cm3            \n"

        " mov r0, r3                        \n"
        " bl Stack_Overflow_Report          \n"

        ".L_no_overflow_cm3:                \n"

        // 保存上下文: EXC_RETURN 和 R4-R11
        " mov r1, lr                        \n" // r1 = EXC_RETURN
        " stmdb r0!, {r1, r4-r11}           \n" // 保存 EXC_RETURN 和 R4-R11
        " str r0, [r3]                      \n" // 保存栈指针到TCB

        // 调用调度器
        " bl MyRTOS_Schedule                \n"

        // 恢复上下文
        " ldr r2, =currentTask              \n"
        " ldr r2, [r2]                      \n" // r2 = 新TCB指针
        " ldr r0, [r2]                      \n" // r0 = 新任务的栈指针

        " ldmia r0!, {r1, r4-r11}           \n" // 恢复 EXC_RETURN 和 R4-R11
        " mov lr, r1                        \n" // LR = EXC_RETURN

        " msr psp, r0                       \n"
        " isb                               \n"

        " bx lr                             \n"

        : /* 无输出操作数 */
        : "i"(TCB_OFFSET_STACK_BASE)
        : "r0", "r1", "r2", "r3", "memory"
    );
}

void SysTick_Handler(void) {
    MyRTOS_Port_EnterCritical();
    int higherPriorityTaskWoken = MyRTOS_Tick_Handler();
    MyRTOS_Port_ExitCritical();
    MyRTOS_Port_YieldFromISR(higherPriorityTaskWoken);
}

// ============================================================================
//                           故障处理
// ============================================================================
void HardFault_Handler(void) __attribute__((naked));
void HardFault_Handler(void) {
    __asm volatile(
        " tst lr, #4                                \n"
        " ite eq                                    \n"
        " mrseq r0, msp                             \n"
        " mrsne r0, psp                             \n"
        " b HardFault_Report                        \n"
    );
}

void HardFault_Report(uint32_t *pulFaultStackAddress) {
    if (MyRTOS_Schedule_IsRunning()) {
        MyRTOS_ReportError(KERNEL_ERROR_HARD_FAULT, pulFaultStackAddress);
    } else {
        Platform_HardFault_Hook(pulFaultStackAddress);
    }
    while (1);
}

void Stack_Overflow_Report(TaskHandle_t pxTask) {
    if (MyRTOS_Schedule_IsRunning()) {
        MyRTOS_ReportError(KERNEL_ERROR_STACK_OVERFLOW, pxTask);
    } else {
        Platform_StackOverflow_Hook(pxTask);
    }
    while (1);
}
