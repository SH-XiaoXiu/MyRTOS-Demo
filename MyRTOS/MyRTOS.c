//
// Created by XiaoXiu on 8/22/2025.
//

#include "MyRTOS.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "gd32f4xx.h"
#include "core_cm4.h"
#include "system_gd32f4xx.h"

//���䶨��
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

//�޴�̬�ڴ�� ģ�� FreeRTOS Heap_1
//====================== ��̬�ڴ���� ======================
#define RTOS_MEMORY_POOL_SIZE (16 * 1024) // ���� 16KB ��̬�ڴ�أ��ɸ�����Ҫ����
static uint8_t rtos_memory_pool[RTOS_MEMORY_POOL_SIZE] __attribute__((aligned(8)));
static size_t pool_next_free_offset = 0;

// �򵥵��ڴ������
void *rtos_malloc(size_t size) {
    // 8�ֽڶ���
    size = (size + 7) & ~7UL;
    if (pool_next_free_offset + size > RTOS_MEMORY_POOL_SIZE) {
        // �ڴ治��
        return NULL;
    }
    void *ptr = &rtos_memory_pool[pool_next_free_offset];
    pool_next_free_offset += size;
    // �����·�����ڴ�
    memset(ptr, 0, size);
    return ptr;
}


//TODO implement rtos_free
void rtos_free(void *ptr) {
    //��֧���ͷ��ڴ档
    // ����ɾ�����ڴ治�ᱻ����
    (void) ptr;
}

//========================================================

#define SIZEOF_TASK_T sizeof(Task_t)

// ȫ�ֱ���
static Task_t *taskListHead = NULL; // ��������ͷָ��
static Task_t *currentTask = NULL; // ��ǰ�������е�����
static Task_t *idleTask = NULL; // ָ����������ָ��
static uint32_t nextTaskId = 0; // ���ڷ���Ψһ������ID

// �ڲ�����������ͨ��ID��������
static Task_t *_find_task_by_id(uint32_t task_id) {
    Task_t *p = taskListHead;
    while (p != NULL) {
        if (p->taskId == task_id) {
            return p;
        }
        p = p->next;
    }
    return NULL;
}

// ��ʼ��RTOS��������������
void MyRTOS_Init(void) {
    taskListHead = NULL;
    currentTask = NULL;
    idleTask = NULL;
    nextTaskId = 0;
    pool_next_free_offset = 0; // <<< ADDED: �����ڴ��ָ��
    DBG_PRINTF("MyRTOS Initialized. Task list and memory pool cleared.\n");
}

void MyRTOS_Idle_Task(void *pv) {
    DBG_PRINTF("Idle Task starting\n");
    while (1) {
        // DBG_PRINTF("Idle task running...\n");
        __WFI(); // ����͹���ģʽ���ȴ��ж�
    }
}

Task_t *Task_Create(void (*func)(void *), void *param) {
    //Ϊ������ƿ� TCB �����ڴ�
    Task_t *t = rtos_malloc(sizeof(Task_t)); // <<< MODIFIED: use rtos_malloc
    if (t == NULL) {
        DBG_PRINTF("Error: Failed to allocate memory for TCB.\n");
        return NULL;
    }

    //Ϊ����ջ�����ڴ�
    uint32_t *stack = rtos_malloc(STACK_SIZE * sizeof(uint32_t)); // <<< MODIFIED: use rtos_malloc
    if (stack == NULL) {
        DBG_PRINTF("Error: Failed to allocate memory for stack.\n");
        //��Ϊ rtos_free �������������޷��ع�TCB�ķ���
        return NULL;
    }

    // ��ʼ��TCB��Ա
    t->func = func;
    t->param = param;
    t->delay = 0;
    t->notification = 0;
    t->is_waiting_notification = 0;
    t->state = TASK_STATE_READY;
    t->taskId = nextTaskId++; // ���䲢����ID
    t->stack_base = stack; // ����ջ����ַ�����ں����ͷ�
    t->next = NULL;

    // ջ��ʼ��
    uint32_t *stk = &stack[STACK_SIZE];
    stk = (uint32_t *) ((uint32_t) stk & ~0x7UL); // 8�ֽڶ���

    // Ӳ���Զ�����ļĴ��� (xPSR, PC, LR, R12, R3-R0)
    stk -= 8;
    stk[7] = 0x01000000; // xPSR (Thumb state)
    stk[6] = (uint32_t) func | 0x1; // PC (Entry point, +1 for Thumb)
    stk[5] = 0xFFFFFFFD; // LR (EXC_RETURN: return to thread mode, use PSP)
    stk[4] = 0x12121212; // R12
    stk[3] = 0x03030303; // R3
    stk[2] = 0x02020202; // R2
    stk[1] = 0x01010101; // R1
    stk[0] = (uint32_t) param; // R0 (Function argument)

    // �����Ҫ�ֶ�����ļĴ��� (R11-R4)
    stk -= 8;
    stk[7] = 0x0B0B0B0B; // R11
    stk[6] = 0x0A0A0A0A; // R10
    stk[5] = 0x09090909; // R9
    stk[4] = 0x08080808; // R8
    stk[3] = 0x07070707; // R7
    stk[2] = 0x06060606; // R6
    stk[1] = 0x05050505; // R5
    stk[0] = 0x04040404; // R4

    t->sp = stk;

    //����������ӵ�����ĩβ
    uint32_t primask_status;
    MY_RTOS_ENTER_CRITICAL(primask_status);
    if (taskListHead == NULL) {
        taskListHead = t;
    } else {
        Task_t *p = taskListHead;
        while (p->next != NULL) {
            p = p->next;
        }
        p->next = t;
    }
    MY_RTOS_EXIT_CRITICAL(primask_status);

    DBG_PRINTF("Task %ld created. Stack top: %p, Initial SP: %p\n", t->taskId, &stack[STACK_SIZE - 1], t->sp);
    return t; // �����ɹ�, ����������
}

int Task_Delete(const Task_t *task_h) {
    if (task_h == NULL) return -1;

    uint32_t primask_status;
    MY_RTOS_ENTER_CRITICAL(primask_status);

    //���������Ƴ�����
    Task_t *prev = NULL;
    Task_t *curr = taskListHead;

    while (curr != NULL && curr != task_h) {
        prev = curr;
        curr = curr->next;
    }

    if (curr == NULL) {
        // ������������
        MY_RTOS_EXIT_CRITICAL(primask_status);
        return -2;
    }

    if (prev == NULL) {
        // ɾ������ͷ�ڵ�
        taskListHead = curr->next;
    } else {
        prev->next = curr->next;
    }

    rtos_free(curr->stack_base);
    rtos_free(curr);

    //���ɾ�����ǵ�ǰ����������������
    if (curr == currentTask) {
        currentTask = NULL; // ��ֹ����ָ��
        MY_RTOS_YIELD();
    }

    MY_RTOS_EXIT_CRITICAL(primask_status);
    DBG_PRINTF("Task %d deleted (memory not reclaimed).\n", task_h->taskId);
    return 0;
}


void Task_Delay(uint32_t tick) {
    if (tick == 0) return;
    uint32_t primask_status;
    MY_RTOS_ENTER_CRITICAL(primask_status);

    currentTask->delay = tick;
    currentTask->state = TASK_STATE_DELAYED;
    MY_RTOS_YIELD();

    MY_RTOS_EXIT_CRITICAL(primask_status);
    __ISB();
}

void *schedule_next_task(void) {
    Task_t *nextTask = NULL;
    Task_t *p = (currentTask == NULL || currentTask->next == NULL) ? taskListHead : currentTask->next;

    // ��ѯ���Ҿ�������
    // ����һ��ѭ���������ƣ���ֹ��û������ʱ��ѭ��
    uint32_t max_loop = nextTaskId + 2;
    while (max_loop--) {
        if (p == NULL) {
            p = taskListHead; // ��ͷ��ʼ
        }
        if (p == NULL) break; // ����Ϊ��

        if (p->state == TASK_STATE_READY) {
            nextTask = p;
            break;
        }
        p = p->next;
    }

    // ���û���ҵ�����������ѡ���������
    if (nextTask == NULL) {
        if (idleTask != NULL && idleTask->state == TASK_STATE_READY) {
            nextTask = idleTask;
        } else {
            // ���ش������������񶼲����ã�ϵͳ����
            DBG_PRINTF("Scheduler: No ready task and idle task is not available!\n");
            // ���ص�ǰ�����SP�������л�ʧ��
            return currentTask ? currentTask->sp : NULL;
        }
    }

    currentTask = nextTask;
    // DBG_PRINTF("Scheduler: Switching to task %ld\n", currentTask->taskId);
    return currentTask->sp;
}


void SysTick_Handler(void) {
    uint32_t primask_status;
    MY_RTOS_ENTER_CRITICAL(primask_status);

    Task_t *p = taskListHead;
    while (p != NULL) {
        // ֻ��������ʱ״̬������
        if (p->state == TASK_STATE_DELAYED) {
            if (p->delay > 0) {
                p->delay--;
            }
            if (p->delay == 0) {
                // ��ʱ����������ָ�Ϊ����̬
                p->state = TASK_STATE_READY;
            }
        }
        p = p->next;
    }

    // �ֶ�����PendSV���е��ȣ��þ��������л��ᱻ����
    MY_RTOS_YIELD();
    MY_RTOS_EXIT_CRITICAL(primask_status);
}

__attribute__((naked)) void PendSV_Handler(void) {
    __asm volatile (
        "mrs r0, psp                   \n" // ��ȡ��ǰ�����ջָ��
        "stmdb r0!, {r4-r11}           \n" // ���� R4-R11 ������ջ

        "ldr r1, =currentTask          \n" // ���� currentTask ָ��ĵ�ַ
        "ldr r3, [r1]                  \n" // ���� currentTask ָ���ֵ (��TCB�ĵ�ַ)
        "cmp r3, #0                    \n" // ��鵱ǰ�����Ƿ�Ϊ NULL
        "beq schedule_and_restore      \n" // ����ǣ�ֱ����������
        "str r0, [r3]                  \n" // �����µ�ջ���� currentTask->sp (sp�ǵ�һ����Ա��ƫ��Ϊ0)

        "schedule_and_restore:         \n"
        "push {lr}                     \n" // ���� LR
        "bl schedule_next_task         \n" // ���õ�������ȡ��һ�������SP������ֵ��R0
        "pop {lr}                      \n"

        "ldmia r0!, {r4-r11}           \n" // ���������ջ�лָ� R4-R11
        "msr psp, r0                   \n" // ���� PSP
        "bx lr                         \n"
    );
}

__attribute__((naked)) void Start_First_Task(void) {
    __asm volatile (
        //��ȡ��һ�������ջ��ָ��
        "ldr r0, =currentTask          \n" // ���� currentTask ָ��ĵ�ַ
        "ldr r2, [r0]                  \n" // ���� currentTask ָ���ֵ (��TCB�ĵ�ַ)
        "ldr r0, [r2]                  \n" // r0 = currentTask->sp

        //�ָ�
        "ldmia r0!, {r4-r11}           \n"

        //PSP
        "msr psp, r0                   \n"

        //�л��� PSP�������߳�ģʽ
        "mov r0, #0x02                 \n"
        "msr control, r0               \n"
        "isb                           \n"

        //ִ���쳣���أ�Ӳ�����Զ��� PSP �ָ�ʣ��Ĵ��� (R0-R3, R12, LR, PC, xPSR) ????????�� ����..
        "bx lr                         \n"
    );
}


void Task_StartScheduler(void) {
    // ������������
    idleTask = Task_Create(MyRTOS_Idle_Task, NULL);
    if (idleTask == NULL) {
        DBG_PRINTF("Error: Failed to create Idle Task!\n");
        while (1);
    }

    SCB->VTOR = (uint32_t) 0x08000000;
    DBG_PRINTF("Starting scheduler...\n");

    //����Ҫ����PenSV��SysTick���ж����ȼ� ��Ȼ���
    NVIC_SetPriority(PendSV_IRQn, 0xFF); // ������ȼ�
    NVIC_SetPriority(SysTick_IRQn, 0xFE); // ��PendSV��һ����

    if (SysTick_Config(SystemCoreClock / 1000)) {
        // 1ms �Ӳ����Լ�������
        DBG_PRINTF("Error: SysTick_Config failed\n");
        while (1);
    }
    // ���õ�ǰ����ΪIdle
    currentTask = idleTask;
    // ��������������������������ٷ���
    Start_First_Task();
    // �����ϲ���ִ�е�����Ĺ� ��Ȼ���ų�ʲô�������ߵ����ڴ淢�����ط�ת. ��߽������Ʋ���������Ӳ��ʹ��ECC�ڴ�
    DBG_PRINTF("Error: Scheduler returned!\n");
    while (1);
}


void HardFault_Handler(void) {
    __disable_irq(); //�ص��ж�
    uint32_t cfsr = SCB->CFSR;
    uint32_t hfsr = SCB->HFSR;
    uint32_t sp;
    __asm volatile ("mrs %0, psp" : "=r" (sp));
    uint32_t stacked_pc = ((uint32_t *) sp)[6];
    DBG_PRINTF("\n!!! Hard Fault !!!\n");
    DBG_PRINTF("CFSR: 0x%08lX, HFSR: 0x%08lX\n", cfsr, hfsr);
    DBG_PRINTF("PSP: 0x%08lX, Stacked PC: 0x%08lX\n", sp, stacked_pc);
    if (cfsr & SCB_CFSR_INVSTATE_Msk)
        DBG_PRINTF("Fault: Invalid State\n");
    if (cfsr & SCB_CFSR_UNDEFINSTR_Msk)
        DBG_PRINTF("Fault: Undefined Instruction\n");
    if (cfsr & SCB_CFSR_IBUSERR_Msk)
        DBG_PRINTF("Fault: Instruction Bus Error\n");
    if (cfsr & SCB_CFSR_PRECISERR_Msk)
        DBG_PRINTF("Fault: Precise Data Bus Error\n");
    while (1);
}

//=================������========================

void Mutex_Init(Mutex_t *mutex) {
    mutex->locked = 0;
    mutex->owner = (uint32_t) -1; // ʹ��ID
    mutex->waiting_mask = 0;
}

void Mutex_Lock(Mutex_t *mutex) {
    uint32_t primask_status;
    MY_RTOS_ENTER_CRITICAL(primask_status);
    while (mutex->locked && mutex->owner != currentTask->taskId) {
        // ��������������У���ǰ������Ҫ�ȴ�
        // �������ID����31��λ�����ʧЧ��������������������ࡣ
        if (currentTask->taskId < 32) {
            mutex->waiting_mask |= (1 << currentTask->taskId);
        }
        currentTask->state = TASK_STATE_BLOCKED; // ���Ϊ����

        MY_RTOS_EXIT_CRITICAL(primask_status);
        SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
        __ISB();
        __WFI(); // �ȴ�������
        // ���Ѻ����¹ر��жϲ��ٴμ����
        MY_RTOS_ENTER_CRITICAL(primask_status);
    }

    // ��ȡ���ɹ�
    mutex->locked = 1;
    mutex->owner = currentTask->taskId;
    if (currentTask->taskId < 32) {
        mutex->waiting_mask &= ~(1 << currentTask->taskId); // �ӵȴ��������Ƴ��Լ�
    }
    MY_RTOS_EXIT_CRITICAL(primask_status);
}

void Mutex_Unlock(Mutex_t *mutex) {
    uint32_t primask_status;
    MY_RTOS_ENTER_CRITICAL(primask_status);
    if (mutex->locked && mutex->owner == currentTask->taskId) {
        mutex->locked = 0;
        mutex->owner = (uint32_t) -1;

        if (mutex->waiting_mask != 0) {
            // �������ڵȴ�������һ��
            Task_t *p = taskListHead;
            while (p != NULL) {
                if (p->taskId < 32 && (mutex->waiting_mask & (1 << p->taskId))) {
                    if (p->state == TASK_STATE_BLOCKED) {
                        p->state = TASK_STATE_READY; // ��������
                    }
                }
                p = p->next;
            }
            MY_RTOS_YIELD();
        }
    }

    MY_RTOS_EXIT_CRITICAL(primask_status);
}

//======================���� ֪ͨ==================

int Task_Notify(uint32_t task_id) {
    Task_t *task_to_notify = _find_task_by_id(task_id);
    if (task_to_notify == NULL) {
        return -1; // ��ЧID
    }

    uint32_t primask_status;
    MY_RTOS_ENTER_CRITICAL(primask_status);

    if (task_to_notify->is_waiting_notification && task_to_notify->state == TASK_STATE_BLOCKED) {
        task_to_notify->is_waiting_notification = 0;
        task_to_notify->state = TASK_STATE_READY; // ����
        MY_RTOS_YIELD();
    }
    MY_RTOS_EXIT_CRITICAL(primask_status);
    return 0;
}

void Task_Wait(void) {
    uint32_t primask_status;
    MY_RTOS_ENTER_CRITICAL(primask_status);
    currentTask->is_waiting_notification = 1;
    currentTask->state = TASK_STATE_BLOCKED; // ����Ϊ����
    MY_RTOS_EXIT_CRITICAL(primask_status);
    MY_RTOS_YIELD();
    __ISB();
}
