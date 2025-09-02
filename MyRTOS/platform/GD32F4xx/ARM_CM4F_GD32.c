#include "MyRTOS.h"
#include "MyRTOS_Kernel_Private.h"
#include "MyRTOS_Port.h"
#include "gd32f4xx.h"
#include "platform.h"

extern TaskHandle_t currentTask;


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

StackType_t *MyRTOS_Port_InitialiseStack(StackType_t *pxTopOfStack, void (*pxCode)(void *), void *pvParameters) {
    pxTopOfStack = (StackType_t *) (((UBaseType_t) pxTopOfStack) & ~((UBaseType_t) 7));

    // 硬件保存的栈帧
    pxTopOfStack--;
    *pxTopOfStack = 0x01000000UL; // xPSR
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

    // 软件保存的栈帧
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
    *pxTopOfStack = 0xFFFFFFFDUL; // EXC_RETURN (PendSV的链接寄存器)

    return pxTopOfStack;
}


// 启用FPU
static void prvEnableFPU(void) {
    SCB->CPACR |= ((3UL << 10 * 2) | (3UL << 11 * 2));
    __DSB();
    __ISB();
}

// 启动调度
BaseType_t MyRTOS_Port_StartScheduler(void) {
    prvEnableFPU();
    NVIC_SetPriority(PendSV_IRQn, (1 << __NVIC_PRIO_BITS) - 1);
    NVIC_SetPriority(SysTick_IRQn, (1 << __NVIC_PRIO_BITS) - 1);
    if (SysTick_Config(SystemCoreClock / MYRTOS_TICK_RATE_HZ) != 0) {
        return 1;
    }
    __asm volatile("svc 0" ::: "memory");
    return 0;
}


void SVC_Handler(void) __attribute__((naked)) {
    __asm volatile(" ldr r0, =currentTask      \n" // 将 currentTask 的地址加载到 r0
                   " ldr r0, [r0]              \n" // 从 currentTask 地址中读取当前任务的 TCB 指针
                   " ldr r0, [r0]              \n" // 从 TCB 中读取任务的栈指针 (sp)

                   " ldmia r0!, {r1, r4-r11}   \n" // 从栈中恢复 EXC_RETURN 到 r1，以及 R4-R11 寄存器
                   " mov lr, r1                \n" // 将 EXC_RETURN 值移动到链接寄存器 LR

                   " msr psp, r0               \n" // 更新 PSP（进程栈指针）为任务的栈指针
                   " isb                       \n" // 指令同步屏障，确保 PSP 更新完成

                   " movs r0, #2               \n" // 设置 CONTROL 寄存器值为 2（使用 PSP 作为当前栈指针）
                   " msr control, r0           \n" // 写入 CONTROL 寄存器
                   " isb                       \n" // 指令同步屏障，确保 CONTROL 更新完成

                   " bx lr                     \n" // 异常返回，硬件自动恢复剩余寄存器（R0-R3、R12、LR、PC、xPSR）
    );
}

void PendSV_Handler(void) __attribute__((naked)) {
    __asm volatile(" mrs r0, psp                       \n" // 获取当前任务的PSP到r0
                   " isb                               \n"

                   " ldr r2, =currentTask              \n"
                   " ldr r3, [r2]                      \n" // r3 = 当前TCB指针（即将切换出去的任务）

                   /*********************************************************************
                    *                     栈溢出检查                                     *
                    * 检查即将切换出去的任务的栈。
                    *********************************************************************/
                   " ldr r1, [r3, %0]                  \n" // r1 = tcb->stack_base，使用偏移宏
                   " cmp r0, r1                        \n" // 比较当前SP(r0)与stack_base(r1)
                   " bge .L_no_overflow                \n" // 如果SP >= stack_base，则安全，跳转。

                   // --- 检测到溢出 ---
                   " mov r0, r3                        \n" // 传递违规任务的句柄(TCB指针)作为第一个参数
                   " bl Stack_Overflow_Report\n" // 调用C处理函数。此函数将停止运行，不会返回。

                   ".L_no_overflow:"
                   /*********************************************************************
                    *                   保存即将切换出去任务的上下文                     *
                    *********************************************************************/

                   " tst lr, #0x10                     \n" // 检查是否需要保存FPU上下文
                   " it eq                             \n"
                   " vstmdbeq r0!, {s16-s31}           \n" // 如果使用了FPU，则保存S16-S31

                   " mov r1, lr                        \n" // 保存EXC_RETURN值
                   " stmdb r0!, {r1, r4-r11}           \n" // 保存软件管理的寄存器(R4-R11, EXC_RETURN)

                   " str r0, [r3]                      \n" // 将最终的栈指针保存回即将切换出去任务的TCB

                   /*********************************************************************
                    *                      调用内核调度器                                *
                    * 调用公共的、抽象的调度器API。此调用将更新'currentTask'全局变量，
                    * 使其指向下一个要运行的任务。
                    *********************************************************************/
                   " bl MyRTOS_Schedule                \n"

                   /*********************************************************************
                    *                   恢复即将切换进来任务的上下文                     *
                    *********************************************************************/

                   " ldr r2, =currentTask              \n"
                   " ldr r2, [r2]                      \n" // r2 = 新TCB指针（即将切换进来的任务）
                   " ldr r0, [r2]                      \n" // r0 = 新任务保存的栈指针

                   " ldmia r0!, {r1, r4-r11}           \n" // 恢复软件管理的寄存器
                   " mov lr, r1                        \n" // 将EXC_RETURN值恢复到LR寄存器

                   " tst lr, #0x10                     \n" // 检查是否需要恢复FPU上下文
                   " it eq                             \n"
                   " vldmiaeq r0!, {s16-s31}           \n" // 如果新任务需要，则恢复S16-S31

                   " msr psp, r0                       \n" // 更新进程栈指针
                   " isb                               \n"
                   " bx lr                             \n" // 异常返回，从栈中恢复PC、PSR、R0-R3、R12

                   : /* 无输出操作数 */
                   : "i"(TCB_OFFSET_STACK_BASE) /* 输入操作数0: stack_base偏移常量 */
                   : "r0", "r1", "r2", "r3", "memory" /* 被破坏的寄存器 */
    );
}


void SysTick_Handler(void) {
    MyRTOS_Port_EnterCritical();
    int higherPriorityTaskWoken = MyRTOS_Tick_Handler();
    MyRTOS_Port_ExitCritical();
    MyRTOS_Port_YieldFromISR(higherPriorityTaskWoken);
}


// 系统异常处理器的入口
void HardFault_Handler(void) __attribute__((naked)) {
    __asm volatile(" tst lr, #4                                \n"
                   " ite eq                                    \n"
                   " mrseq r0, msp                             \n"
                   " mrsne r0, psp                             \n"
                   " b HardFault_Report                 \n");
}


void HardFault_Report(uint32_t *pulFaultStackAddress) {
    if (MyRTOS_Schedule_IsRunning()) {
        MyRTOS_ReportError(KERNEL_ERROR_HARD_FAULT, pulFaultStackAddress);
    } else {
        Platform_HardFault_Hook(pulFaultStackAddress);
    }
    while (1)
        ;
}


void Stack_Overflow_Report(TaskHandle_t pxTask) {
    if (MyRTOS_Schedule_IsRunning()) {
        MyRTOS_ReportError(KERNEL_ERROR_STACK_OVERFLOW, pxTask);
    } else {
        Platform_StackOverflow_Hook(pxTask);
    }
    while (1)
        ;
}
