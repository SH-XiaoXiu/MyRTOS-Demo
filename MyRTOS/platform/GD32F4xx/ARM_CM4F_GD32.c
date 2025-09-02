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

    // Ӳ�������ջ֡
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

    // ��������ջ֡
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
    *pxTopOfStack = 0xFFFFFFFDUL; // EXC_RETURN (PendSV�����ӼĴ���)

    return pxTopOfStack;
}


// ����FPU
static void prvEnableFPU(void) {
    SCB->CPACR |= ((3UL << 10 * 2) | (3UL << 11 * 2));
    __DSB();
    __ISB();
}

// ��������
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
    __asm volatile(" ldr r0, =currentTask      \n" // �� currentTask �ĵ�ַ���ص� r0
                   " ldr r0, [r0]              \n" // �� currentTask ��ַ�ж�ȡ��ǰ����� TCB ָ��
                   " ldr r0, [r0]              \n" // �� TCB �ж�ȡ�����ջָ�� (sp)

                   " ldmia r0!, {r1, r4-r11}   \n" // ��ջ�лָ� EXC_RETURN �� r1���Լ� R4-R11 �Ĵ���
                   " mov lr, r1                \n" // �� EXC_RETURN ֵ�ƶ������ӼĴ��� LR

                   " msr psp, r0               \n" // ���� PSP������ջָ�룩Ϊ�����ջָ��
                   " isb                       \n" // ָ��ͬ�����ϣ�ȷ�� PSP �������

                   " movs r0, #2               \n" // ���� CONTROL �Ĵ���ֵΪ 2��ʹ�� PSP ��Ϊ��ǰջָ�룩
                   " msr control, r0           \n" // д�� CONTROL �Ĵ���
                   " isb                       \n" // ָ��ͬ�����ϣ�ȷ�� CONTROL �������

                   " bx lr                     \n" // �쳣���أ�Ӳ���Զ��ָ�ʣ��Ĵ�����R0-R3��R12��LR��PC��xPSR��
    );
}

void PendSV_Handler(void) __attribute__((naked)) {
    __asm volatile(" mrs r0, psp                       \n" // ��ȡ��ǰ�����PSP��r0
                   " isb                               \n"

                   " ldr r2, =currentTask              \n"
                   " ldr r3, [r2]                      \n" // r3 = ��ǰTCBָ�루�����л���ȥ������

                   /*********************************************************************
                    *                     ջ������                                     *
                    * ��鼴���л���ȥ�������ջ��
                    *********************************************************************/
                   " ldr r1, [r3, %0]                  \n" // r1 = tcb->stack_base��ʹ��ƫ�ƺ�
                   " cmp r0, r1                        \n" // �Ƚϵ�ǰSP(r0)��stack_base(r1)
                   " bge .L_no_overflow                \n" // ���SP >= stack_base����ȫ����ת��

                   // --- ��⵽��� ---
                   " mov r0, r3                        \n" // ����Υ������ľ��(TCBָ��)��Ϊ��һ������
                   " bl Stack_Overflow_Report\n" // ����C���������˺�����ֹͣ���У����᷵�ء�

                   ".L_no_overflow:"
                   /*********************************************************************
                    *                   ���漴���л���ȥ�����������                     *
                    *********************************************************************/

                   " tst lr, #0x10                     \n" // ����Ƿ���Ҫ����FPU������
                   " it eq                             \n"
                   " vstmdbeq r0!, {s16-s31}           \n" // ���ʹ����FPU���򱣴�S16-S31

                   " mov r1, lr                        \n" // ����EXC_RETURNֵ
                   " stmdb r0!, {r1, r4-r11}           \n" // �����������ļĴ���(R4-R11, EXC_RETURN)

                   " str r0, [r3]                      \n" // �����յ�ջָ�뱣��ؼ����л���ȥ�����TCB

                   /*********************************************************************
                    *                      �����ں˵�����                                *
                    * ���ù����ġ�����ĵ�����API���˵��ý�����'currentTask'ȫ�ֱ�����
                    * ʹ��ָ����һ��Ҫ���е�����
                    *********************************************************************/
                   " bl MyRTOS_Schedule                \n"

                   /*********************************************************************
                    *                   �ָ������л����������������                     *
                    *********************************************************************/

                   " ldr r2, =currentTask              \n"
                   " ldr r2, [r2]                      \n" // r2 = ��TCBָ�루�����л�����������
                   " ldr r0, [r2]                      \n" // r0 = �����񱣴��ջָ��

                   " ldmia r0!, {r1, r4-r11}           \n" // �ָ��������ļĴ���
                   " mov lr, r1                        \n" // ��EXC_RETURNֵ�ָ���LR�Ĵ���

                   " tst lr, #0x10                     \n" // ����Ƿ���Ҫ�ָ�FPU������
                   " it eq                             \n"
                   " vldmiaeq r0!, {s16-s31}           \n" // �����������Ҫ����ָ�S16-S31

                   " msr psp, r0                       \n" // ���½���ջָ��
                   " isb                               \n"
                   " bx lr                             \n" // �쳣���أ���ջ�лָ�PC��PSR��R0-R3��R12

                   : /* ����������� */
                   : "i"(TCB_OFFSET_STACK_BASE) /* ���������0: stack_baseƫ�Ƴ��� */
                   : "r0", "r1", "r2", "r3", "memory" /* ���ƻ��ļĴ��� */
    );
}


void SysTick_Handler(void) {
    MyRTOS_Port_EnterCritical();
    int higherPriorityTaskWoken = MyRTOS_Tick_Handler();
    MyRTOS_Port_ExitCritical();
    MyRTOS_Port_YieldFromISR(higherPriorityTaskWoken);
}


// ϵͳ�쳣�����������
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
