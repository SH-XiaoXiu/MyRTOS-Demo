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

//补充定义
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

//巨大静态内存池 模仿 FreeRTOS Heap_1
//====================== 静态内存管理 ======================
#define RTOS_MEMORY_POOL_SIZE (16 * 1024) // 定义 16KB 静态内存池，可根据需要调整
static uint8_t rtos_memory_pool[RTOS_MEMORY_POOL_SIZE] __attribute__((aligned(8)));
static size_t pool_next_free_offset = 0;

// 简单的内存分配器
void *rtos_malloc(size_t size) {
    // 8字节对齐
    size = (size + 7) & ~7UL;
    if (pool_next_free_offset + size > RTOS_MEMORY_POOL_SIZE) {
        // 内存不足
        return NULL;
    }
    void *ptr = &rtos_memory_pool[pool_next_free_offset];
    pool_next_free_offset += size;
    // 清零新分配的内存
    memset(ptr, 0, size);
    return ptr;
}


//TODO implement rtos_free
void rtos_free(void *ptr) {
    //不支持释放内存。
    // 任务删除后内存不会被回收
    (void) ptr;
}

//========================================================

#define SIZEOF_TASK_T sizeof(Task_t)

// 全局变量
static Task_t *taskListHead = NULL; // 任务链表头指针
static Task_t *currentTask = NULL; // 当前正在运行的任务
static Task_t *idleTask = NULL; // 指向空闲任务的指针
static uint32_t nextTaskId = 0; // 用于分配唯一的任务ID

// 内部辅助函数，通过ID查找任务
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

// 初始化RTOS，清空所有任务槽
void MyRTOS_Init(void) {
    taskListHead = NULL;
    currentTask = NULL;
    idleTask = NULL;
    nextTaskId = 0;
    pool_next_free_offset = 0; // <<< ADDED: 重置内存池指针
    DBG_PRINTF("MyRTOS Initialized. Task list and memory pool cleared.\n");
}

void MyRTOS_Idle_Task(void *pv) {
    DBG_PRINTF("Idle Task starting\n");
    while (1) {
        // DBG_PRINTF("Idle task running...\n");
        __WFI(); // 进入低功耗模式，等待中断
    }
}

Task_t *Task_Create(void (*func)(void *), void *param) {
    //为任务控制块 TCB 分配内存
    Task_t *t = rtos_malloc(sizeof(Task_t)); // <<< MODIFIED: use rtos_malloc
    if (t == NULL) {
        DBG_PRINTF("Error: Failed to allocate memory for TCB.\n");
        return NULL;
    }

    //为任务栈分配内存
    uint32_t *stack = rtos_malloc(STACK_SIZE * sizeof(uint32_t)); // <<< MODIFIED: use rtos_malloc
    if (stack == NULL) {
        DBG_PRINTF("Error: Failed to allocate memory for stack.\n");
        //因为 rtos_free 不工作，这里无法回滚TCB的分配
        return NULL;
    }

    // 初始化TCB成员
    t->func = func;
    t->param = param;
    t->delay = 0;
    t->notification = 0;
    t->is_waiting_notification = 0;
    t->state = TASK_STATE_READY;
    t->taskId = nextTaskId++; // 分配并递增ID
    t->stack_base = stack; // 保存栈基地址，用于后续释放
    t->next = NULL;

    // 栈初始化
    uint32_t *stk = &stack[STACK_SIZE];
    stk = (uint32_t *) ((uint32_t) stk & ~0x7UL); // 8字节对齐

    // 硬件自动保存的寄存器 (xPSR, PC, LR, R12, R3-R0)
    stk -= 8;
    stk[7] = 0x01000000; // xPSR (Thumb state)
    stk[6] = (uint32_t) func | 0x1; // PC (Entry point, +1 for Thumb)
    stk[5] = 0xFFFFFFFD; // LR (EXC_RETURN: return to thread mode, use PSP)
    stk[4] = 0x12121212; // R12
    stk[3] = 0x03030303; // R3
    stk[2] = 0x02020202; // R2
    stk[1] = 0x01010101; // R1
    stk[0] = (uint32_t) param; // R0 (Function argument)

    // 软件需要手动保存的寄存器 (R11-R4)
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

    //将新任务添加到链表末尾
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
    return t; // 创建成功, 返回任务句柄
}

int Task_Delete(const Task_t *task_h) {
    if (task_h == NULL) return -1;

    uint32_t primask_status;
    MY_RTOS_ENTER_CRITICAL(primask_status);

    //从链表中移除任务
    Task_t *prev = NULL;
    Task_t *curr = taskListHead;

    while (curr != NULL && curr != task_h) {
        prev = curr;
        curr = curr->next;
    }

    if (curr == NULL) {
        // 任务不在链表中
        MY_RTOS_EXIT_CRITICAL(primask_status);
        return -2;
    }

    if (prev == NULL) {
        // 删除的是头节点
        taskListHead = curr->next;
    } else {
        prev->next = curr->next;
    }

    rtos_free(curr->stack_base);
    rtos_free(curr);

    //如果删除的是当前任务，立即触发调度
    if (curr == currentTask) {
        currentTask = NULL; // 防止悬空指针
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

    // 轮询查找就绪任务
    // 增加一个循环次数限制，防止在没有任务时死循环
    uint32_t max_loop = nextTaskId + 2;
    while (max_loop--) {
        if (p == NULL) {
            p = taskListHead; // 从头开始
        }
        if (p == NULL) break; // 链表为空

        if (p->state == TASK_STATE_READY) {
            nextTask = p;
            break;
        }
        p = p->next;
    }

    // 如果没有找到就绪任务，则选择空闲任务
    if (nextTask == NULL) {
        if (idleTask != NULL && idleTask->state == TASK_STATE_READY) {
            nextTask = idleTask;
        } else {
            // 严重错误：连空闲任务都不可用，系统卡死
            DBG_PRINTF("Scheduler: No ready task and idle task is not available!\n");
            // 返回当前任务的SP，避免切换失败
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
        // 只处理处于延时状态的任务
        if (p->state == TASK_STATE_DELAYED) {
            if (p->delay > 0) {
                p->delay--;
            }
            if (p->delay == 0) {
                // 延时结束，任务恢复为就绪态
                p->state = TASK_STATE_READY;
            }
        }
        p = p->next;
    }

    // 手动触发PendSV进行调度，让就绪任务有机会被调用
    MY_RTOS_YIELD();
    MY_RTOS_EXIT_CRITICAL(primask_status);
}

__attribute__((naked)) void PendSV_Handler(void) {
    __asm volatile (
        "mrs r0, psp                   \n" // 获取当前任务的栈指针
        "stmdb r0!, {r4-r11}           \n" // 保存 R4-R11 到任务栈

        "ldr r1, =currentTask          \n" // 加载 currentTask 指针的地址
        "ldr r3, [r1]                  \n" // 加载 currentTask 指针的值 (即TCB的地址)
        "cmp r3, #0                    \n" // 检查当前任务是否为 NULL
        "beq schedule_and_restore      \n" // 如果是，直接跳到调度
        "str r0, [r3]                  \n" // 保存新的栈顶到 currentTask->sp (sp是第一个成员，偏移为0)

        "schedule_and_restore:         \n"
        "push {lr}                     \n" // 保存 LR
        "bl schedule_next_task         \n" // 调用调度器获取下一个任务的SP，返回值在R0
        "pop {lr}                      \n"

        "ldmia r0!, {r4-r11}           \n" // 从新任务的栈中恢复 R4-R11
        "msr psp, r0                   \n" // 更新 PSP
        "bx lr                         \n"
    );
}

__attribute__((naked)) void Start_First_Task(void) {
    __asm volatile (
        //获取第一个任务的栈顶指针
        "ldr r0, =currentTask          \n" // 加载 currentTask 指针的地址
        "ldr r2, [r0]                  \n" // 加载 currentTask 指针的值 (即TCB的地址)
        "ldr r0, [r2]                  \n" // r0 = currentTask->sp

        //恢复
        "ldmia r0!, {r4-r11}           \n"

        //PSP
        "msr psp, r0                   \n"

        //切换到 PSP，进入线程模式
        "mov r0, #0x02                 \n"
        "msr control, r0               \n"
        "isb                           \n"

        //执行异常返回，硬件会自动从 PSP 恢复剩余寄存器 (R0-R3, R12, LR, PC, xPSR) ????????额 逆天..
        "bx lr                         \n"
    );
}


void Task_StartScheduler(void) {
    // 创建空闲任务
    idleTask = Task_Create(MyRTOS_Idle_Task, NULL);
    if (idleTask == NULL) {
        DBG_PRINTF("Error: Failed to create Idle Task!\n");
        while (1);
    }

    SCB->VTOR = (uint32_t) 0x08000000;
    DBG_PRINTF("Starting scheduler...\n");

    //这里要设置PenSV和SysTick的中断优先级 不然会寄
    NVIC_SetPriority(PendSV_IRQn, 0xFF); // 最低优先级
    NVIC_SetPriority(SysTick_IRQn, 0xFE); // 比PendSV高一级喵

    if (SysTick_Config(SystemCoreClock / 1000)) {
        // 1ms 嫌不够自己改上面
        DBG_PRINTF("Error: SysTick_Config failed\n");
        while (1);
    }
    // 设置当前任务为Idle
    currentTask = idleTask;
    // 调用启动函数，这个函数将不再返回
    Start_First_Task();
    // 理论上不会执行到这里的哈 当然不排除什么宇宙射线导致内存发生比特反转. 这边建议您移步至服务器硬件使用ECC内存
    DBG_PRINTF("Error: Scheduler returned!\n");
    while (1);
}


void HardFault_Handler(void) {
    __disable_irq(); //关掉中断
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

//=================互斥锁========================

void Mutex_Init(Mutex_t *mutex) {
    mutex->locked = 0;
    mutex->owner = (uint32_t) -1; // 使用ID
    mutex->waiting_mask = 0;
}

void Mutex_Lock(Mutex_t *mutex) {
    uint32_t primask_status;
    MY_RTOS_ENTER_CRITICAL(primask_status);
    while (mutex->locked && mutex->owner != currentTask->taskId) {
        // 锁被其他任务持有，当前任务需要等待
        // 如果任务ID超过31，位掩码会失效。这里假设任务数量不多。
        if (currentTask->taskId < 32) {
            mutex->waiting_mask |= (1 << currentTask->taskId);
        }
        currentTask->state = TASK_STATE_BLOCKED; // 标记为阻塞

        MY_RTOS_EXIT_CRITICAL(primask_status);
        SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
        __ISB();
        __WFI(); // 等待被唤醒
        // 唤醒后，重新关闭中断并再次检查锁
        MY_RTOS_ENTER_CRITICAL(primask_status);
    }

    // 获取锁成功
    mutex->locked = 1;
    mutex->owner = currentTask->taskId;
    if (currentTask->taskId < 32) {
        mutex->waiting_mask &= ~(1 << currentTask->taskId); // 从等待掩码中移除自己
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
            // 有任务在等待，唤醒一个
            Task_t *p = taskListHead;
            while (p != NULL) {
                if (p->taskId < 32 && (mutex->waiting_mask & (1 << p->taskId))) {
                    if (p->state == TASK_STATE_BLOCKED) {
                        p->state = TASK_STATE_READY; // 唤醒任务
                    }
                }
                p = p->next;
            }
            MY_RTOS_YIELD();
        }
    }

    MY_RTOS_EXIT_CRITICAL(primask_status);
}

//======================挂起 通知==================

int Task_Notify(uint32_t task_id) {
    Task_t *task_to_notify = _find_task_by_id(task_id);
    if (task_to_notify == NULL) {
        return -1; // 无效ID
    }

    uint32_t primask_status;
    MY_RTOS_ENTER_CRITICAL(primask_status);

    if (task_to_notify->is_waiting_notification && task_to_notify->state == TASK_STATE_BLOCKED) {
        task_to_notify->is_waiting_notification = 0;
        task_to_notify->state = TASK_STATE_READY; // 唤醒
        MY_RTOS_YIELD();
    }
    MY_RTOS_EXIT_CRITICAL(primask_status);
    return 0;
}

void Task_Wait(void) {
    uint32_t primask_status;
    MY_RTOS_ENTER_CRITICAL(primask_status);
    currentTask->is_waiting_notification = 1;
    currentTask->state = TASK_STATE_BLOCKED; // 设置为阻塞
    MY_RTOS_EXIT_CRITICAL(primask_status);
    MY_RTOS_YIELD();
    __ISB();
}
