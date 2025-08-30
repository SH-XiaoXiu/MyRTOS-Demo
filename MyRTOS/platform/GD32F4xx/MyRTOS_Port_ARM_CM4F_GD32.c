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

//�� MyRTOS.c ����
extern TaskHandle_t currentTask;

// �ⲿ����
extern void *schedule_next_task(void);


static void enableFPU(void);

void MyRTOS_Idle_Task(void *pv) {
    while (1) {
        __WFI();
    }
}

/**
 * @brief ��ʼ�������ջ֡
 */
StackType_t *MyRTOS_Port_InitialiseStack(StackType_t *pxTopOfStack, void (*pxCode)(void *), void *pvParameters) {
    // ƫ��ջ��ָ�룬ȷ��8�ֽڶ���
    pxTopOfStack = (StackType_t *) (((uint32_t) pxTopOfStack) & ~0x07UL);

    // Ӳ���Զ������֡ (R0-R3, R12, LR, PC, xPSR)
    pxTopOfStack--;
    *pxTopOfStack = 0x01000000; // xPSR (Thumb state)
    pxTopOfStack--;
    *pxTopOfStack = ((uint32_t) pxCode) | 1u; // PC (with Thumb bit)
    pxTopOfStack--;
    *pxTopOfStack = 0; // LR (����Ӧ����)
    pxTopOfStack--;
    *pxTopOfStack = 0x12121212; // R12 (����ֵ)
    pxTopOfStack--;
    *pxTopOfStack = 0x03030303; // R3 (����ֵ)
    pxTopOfStack--;
    *pxTopOfStack = 0x02020202; // R2 (����ֵ)
    pxTopOfStack--;
    *pxTopOfStack = 0x01010101; // R1 (����ֵ)
    pxTopOfStack--;
    *pxTopOfStack = (uint32_t) pvParameters; // R0 (�������)

    // ����ֶ������֡ (R4-R11, EXC_RETURN)
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
    // EXC_RETURN: �����߳�ģʽ, ʹ��PSP��
    // ��4λΪ1��ʾջ��û��FPU���ݣ�Ϊ0��ʾ�С�
    // ��ʼʱ�ٶ�û�У�FPU�����Ľ������ء�
    *pxTopOfStack = 0xFFFFFFFD;

    return pxTopOfStack;
}


/**
 * @brief ����FPU
 */
static void enableFPU(void) {
    SCB->CPACR |= ((3UL << 10 * 2) | (3UL << 11 * 2));
    __ISB();
    __DSB();
}

/**
 * @brief ����RTOS������
 */
BaseType_t MyRTOS_Port_StartScheduler(void) {
    // ���� FPU
    enableFPU();

    // ���� PendSV �� SysTick �����ȼ�
    NVIC_SetPriority(PendSV_IRQn, 255);
    NVIC_SetPriority(SysTick_IRQn, 15);

    // ���� SysTick ��ʱ��
    if (SysTick_Config(SystemCoreClock / MY_RTOS_TICK_RATE_HZ)) {
        while (1);
    }

    // ��һ�ε���,�ֶ�ѡ����һ������
    schedule_next_task();

    // ����SVC�жϣ�������һ������
    __asm volatile("svc 0");

    return -1;
}

/**
 * @brief SVC Handler (����ԭʼջ֡ģ�ͺ�FPU)
 */
__attribute__((naked)) void SVC_Handler(void) {
    __asm volatile (
        " ldr r0, =currentTask      \n"
        " ldr r0, [r0]              \n"
        " ldr r0, [r0]              \n" // r0 = �����ջָ��(sp)

        " ldmia r0!, {r1, r4-r11}   \n" // ��ջ�ϻָ� EXC_RETURN (��r1) �� R4-R11
        " mov lr, r1                \n" // ��EXC_RETURNֵ����LR�Ĵ���

        " msr psp, r0               \n" // ����PSP��ʹ��ָ��Ӳ�������ջ֡
        " isb                       \n"

        " movs r0, #2               \n" // ����CONTROL�Ĵ������л���PSP
        " msr control, r0           \n"
        " isb                       \n"

        " bx lr                     \n" // �쳣���أ�Ӳ�����Զ���PSP�ָ�ʣ��Ĵ���
    );
}

/**
 * @brief PendSV Handler (����ԭʼջ֡ģ�ͺ�FPU)
 */
__attribute__((naked)) void PendSV_Handler(void) {
    __asm volatile (
        " mrs r0, psp                       \n" // ��ȡ��ǰ�����PSP
        " isb                               \n"

        " ldr r2, =currentTask              \n"
        " ldr r3, [r2]                      \n" // r3 = ��ǰ�����TCB

        " tst lr, #0x10                     \n" // ���LR[4]���ж��Ƿ�ʹ����FPU
        " it eq                             \n"
        " vstmdbeq r0!, {s16-s31}           \n" // ���ʹ���ˣ�����FPU�Ĵ���

        " mov r1, lr                        \n" // ��LR(EXC_RETURN)���浽r1
        " stmdb r0!, {r1, r4-r11}           \n" // �������������

        " str r0, [r3]                      \n" // ���µ�SP�����TCB

        " bl schedule_next_task             \n" // ѡ����һ������

        " ldr r2, =currentTask              \n"
        " ldr r2, [r2]                      \n" // r2 = �������TCB
        " ldr r0, [r2]                      \n" // r0 = �������SP

        " ldmia r0!, {r1, r4-r11}           \n" // �ָ����������
        " mov lr, r1                        \n" // �ָ�EXC_RETURN��LR

        " tst lr, #0x10                     \n" // ����������Ƿ�ʹ����FPU
        " it eq                             \n"
        " vldmiaeq r0!, {s16-s31}           \n" // ����ǣ��ָ�FPU�Ĵ���

        " msr psp, r0                       \n" // ����PSP
        " isb                               \n"
        " bx lr                             \n" // �쳣����
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
 * @brief HardFault Handler (������API)
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
