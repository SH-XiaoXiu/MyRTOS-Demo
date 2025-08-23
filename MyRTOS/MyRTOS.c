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

//====================== 动态内存管理 ======================

#define RTOS_MEMORY_POOL_SIZE (16 * 1024)
#define HEAP_BYTE_ALIGNMENT   8

/* 内存块的头部结构，用于构建空闲内存块链表 */
typedef struct BlockLink_t {
    struct BlockLink_t *pxNextFreeBlock; /* 指向链表中下一个空闲内存块 */
    size_t xBlockSize; /* 当前内存块的大小(包含头部), 最高位用作分配标记 */
} BlockLink_t;

/* 内存块头部结构的大小 (已对齐) */
static const size_t xHeapStructSize = (sizeof(BlockLink_t) + (HEAP_BYTE_ALIGNMENT - 1)) & ~(
                                          (size_t) HEAP_BYTE_ALIGNMENT - 1);
/* 最小内存块大小，一个块必须能容纳分裂后的两个头部 */
#define HEAP_MINIMUM_BLOCK_SIZE    (xHeapStructSize * 2)

/* 静态内存池 */
static uint8_t rtos_memory_pool[RTOS_MEMORY_POOL_SIZE] __attribute__((aligned(HEAP_BYTE_ALIGNMENT)));
/* 空闲链表的起始和结束标记 */
static BlockLink_t xStart, *pxEnd = NULL;
/* 剩余可用内存大小 */
static size_t xFreeBytesRemaining = 0U;
/* 用于标记内存块是否已被分配的标志位 */
static size_t xBlockAllocatedBit = 0;

/* 初始化堆内存 (仅在首次分配时调用) */
static void prvHeapInit(void) {
    BlockLink_t *pxFirstFreeBlock;
    uint8_t *pucAlignedHeap;
    size_t uxAddress = (size_t) rtos_memory_pool;
    size_t xTotalHeapSize = RTOS_MEMORY_POOL_SIZE;

    if ((uxAddress & (HEAP_BYTE_ALIGNMENT - 1)) != 0) {
        uxAddress += (HEAP_BYTE_ALIGNMENT - (uxAddress & (HEAP_BYTE_ALIGNMENT - 1)));
        xTotalHeapSize -= uxAddress - (size_t) rtos_memory_pool;
    }
    pucAlignedHeap = (uint8_t *) uxAddress;

    xStart.pxNextFreeBlock = (BlockLink_t *) pucAlignedHeap;
    xStart.xBlockSize = (size_t) 0;

    uxAddress = ((size_t) pucAlignedHeap) + xTotalHeapSize - xHeapStructSize;
    pxEnd = (BlockLink_t *) uxAddress;
    pxEnd->xBlockSize = 0;
    pxEnd->pxNextFreeBlock = NULL;

    pxFirstFreeBlock = (BlockLink_t *) pucAlignedHeap;
    pxFirstFreeBlock->xBlockSize = uxAddress - (size_t) pxFirstFreeBlock;
    pxFirstFreeBlock->pxNextFreeBlock = pxEnd;

    xFreeBytesRemaining = pxFirstFreeBlock->xBlockSize;
    xBlockAllocatedBit = ((size_t) 1) << ((sizeof(size_t) * 8) - 1);
}

/* 将一个内存块插入到空闲链表中，并处理与相邻空闲块的合并 */
static void prvInsertBlockIntoFreeList(BlockLink_t *pxBlockToInsert) {
    BlockLink_t *pxIterator;
    uint8_t *puc;

    /* 遍历链表，找到可以插入新释放块的位置 (按地址排序) */
    for (pxIterator = &xStart; pxIterator->pxNextFreeBlock < pxBlockToInsert;
         pxIterator = pxIterator->pxNextFreeBlock) {
        /* 什么都不用做，只是找到位置 */
    }

    /* 看看新块是否与迭代器指向的块在物理上相邻 */
    puc = (uint8_t *) pxIterator;
    if ((puc + pxIterator->xBlockSize) == (uint8_t *) pxBlockToInsert) {
        /* 相邻，合并它们 */
        pxIterator->xBlockSize += pxBlockToInsert->xBlockSize;
        /* 合并后，新块就是迭代器指向的块了 */
        pxBlockToInsert = pxIterator;
    } else {
        /* 不相邻，将新块链接到迭代器后面 */
        pxBlockToInsert->pxNextFreeBlock = pxIterator->pxNextFreeBlock;
    }

    /* 看看新块是否与后面的块相邻 */
    puc = (uint8_t *) pxBlockToInsert;
    if ((puc + pxBlockToInsert->xBlockSize) == (uint8_t *) pxIterator->pxNextFreeBlock) {
        if (pxIterator->pxNextFreeBlock != pxEnd) {
            /* 相邻，合并它们 */
            pxBlockToInsert->xBlockSize += pxIterator->pxNextFreeBlock->xBlockSize;
            pxBlockToInsert->pxNextFreeBlock = pxIterator->pxNextFreeBlock->pxNextFreeBlock;
        }
    }

    /* 如果第一步没有合并，那么迭代器的下一个节点需要指向新块 */
    if (pxIterator != pxBlockToInsert) {
        pxIterator->pxNextFreeBlock = pxBlockToInsert;
    }
}

/* 动态内存分配函数 */
void *rtos_malloc(size_t xWantedSize) {
    BlockLink_t *pxBlock, *pxPreviousBlock, *pxNewBlockLink;
    void *pvReturn = NULL;
    uint32_t primask_status;

    MY_RTOS_ENTER_CRITICAL(primask_status); {
        if (pxEnd == NULL) {
            prvHeapInit();
        }

        if ((xWantedSize > 0) && ((xWantedSize & xBlockAllocatedBit) == 0)) {
            size_t xTotalSize = xHeapStructSize + xWantedSize;
            if ((xTotalSize & (HEAP_BYTE_ALIGNMENT - 1)) != 0) {
                xTotalSize += (HEAP_BYTE_ALIGNMENT - (xTotalSize & (HEAP_BYTE_ALIGNMENT - 1)));
            }

            if (xTotalSize <= xFreeBytesRemaining) {
                pxPreviousBlock = &xStart;
                pxBlock = xStart.pxNextFreeBlock;
                while ((pxBlock->xBlockSize < xTotalSize) && (pxBlock->pxNextFreeBlock != NULL)) {
                    pxPreviousBlock = pxBlock;
                    pxBlock = pxBlock->pxNextFreeBlock;
                }

                if (pxBlock != pxEnd) {
                    pvReturn = (void *) (((uint8_t *) pxBlock) + xHeapStructSize);
                    pxPreviousBlock->pxNextFreeBlock = pxBlock->pxNextFreeBlock;

                    if ((pxBlock->xBlockSize - xTotalSize) > HEAP_MINIMUM_BLOCK_SIZE) {
                        pxNewBlockLink = (BlockLink_t *) (((uint8_t *) pxBlock) + xTotalSize);
                        pxNewBlockLink->xBlockSize = pxBlock->xBlockSize - xTotalSize;
                        pxBlock->xBlockSize = xTotalSize;
                        prvInsertBlockIntoFreeList(pxNewBlockLink);
                    }

                    xFreeBytesRemaining -= pxBlock->xBlockSize;
                    pxBlock->xBlockSize |= xBlockAllocatedBit;
                    pxBlock->pxNextFreeBlock = NULL;
                }
            }
        }
    }
    MY_RTOS_EXIT_CRITICAL(primask_status);

    return pvReturn;
}

/* 动态内存释放函数 */
void rtos_free(void *pv) {
    if (pv == NULL) return;

    uint8_t *puc = (uint8_t *) pv;
    BlockLink_t *pxLink;
    uint32_t primask_status;

    puc -= xHeapStructSize;
    pxLink = (BlockLink_t *) puc;

    if (((pxLink->xBlockSize & xBlockAllocatedBit) != 0) && (pxLink->pxNextFreeBlock == NULL)) {
        pxLink->xBlockSize &= ~xBlockAllocatedBit;
        MY_RTOS_ENTER_CRITICAL(primask_status); {
            xFreeBytesRemaining += pxLink->xBlockSize;
            prvInsertBlockIntoFreeList(pxLink);
        }
        MY_RTOS_EXIT_CRITICAL(primask_status);
    }
}

//================================================================================

// 全局变量
static Task_t *taskListHead = NULL;
static Task_t *currentTask = NULL;
static Task_t *idleTask = NULL;
static uint32_t nextTaskId = 0;


void MyRTOS_Init(void) {
    taskListHead = NULL;
    currentTask = NULL;
    idleTask = NULL;
    nextTaskId = 0;
    DBG_PRINTF("MyRTOS Initialized. Task list cleared and memory manager reset.\n");
}

//================= Task ================
#define SIZEOF_TASK_T sizeof(Task_t)

//通过ID查找任务
static Task_t *find_task_by_id(uint32_t task_id) {
    Task_t *p = taskListHead;
    while (p != NULL) {
        if (p->taskId == task_id) return p;
        p = p->next;
    }
    return NULL;
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
    Task_t *t = rtos_malloc(sizeof(Task_t));
    if (t == NULL) {
        DBG_PRINTF("Error: Failed to allocate memory for TCB.\n");
        return NULL;
    }

    //为任务栈分配内存
    uint32_t stack_size_bytes = STACK_SIZE * sizeof(uint32_t);
    uint32_t *stack = rtos_malloc(stack_size_bytes);
    if (stack == NULL) {
        DBG_PRINTF("Error: Failed to allocate memory for stack.\n");
        rtos_free(t); // <<< ADDED: 如果栈分配失败，回滚TCB的分配
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

    uint32_t *sp = stack + STACK_SIZE;
    sp = (uint32_t *) (((uintptr_t) sp) & ~0x7u); // 8 字节对齐

    /* 先写硬件自动保存帧（硬件在 EXC_RETURN 时弹出） */
    sp -= 8;
    sp[0] = (uint32_t) param; // R0
    sp[1] = 0x01010101; // R1
    sp[2] = 0x02020202; // R2
    sp[3] = 0x03030303; // R3
    sp[4] = 0x12121212; // R12
    sp[5] = 0x00000000; // LR (任务返回)
    sp[6] = ((uint32_t) func) | 1u; // PC (Thumb)
    sp[7] = 0x01000000; // xPSR (T bit)

    /* 再写软件保存区（PendSV/SVC 恢复 R4-R11） */
    sp -= 8;
    sp[0] = 0x04040404;
    sp[1] = 0x05050505;
    sp[2] = 0x06060606;
    sp[3] = 0x07070707;
    sp[4] = 0x08080808;
    sp[5] = 0x09090909;
    sp[6] = 0x0A0A0A0A;
    sp[7] = 0x0B0B0B0B;
    t->sp = sp;

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

    DBG_PRINTF("Task %lu created. Stack top: %p, Initial SP: %p\n", t->taskId, &stack[STACK_SIZE - 1], t->sp);
    return t; // 创建成功, 返回任务句柄
}

// 删除任务
int Task_Delete(const Task_t *task_h) {
    // 不允许删除 NULL 任务或空闲任务
    if (task_h == NULL || task_h == idleTask) {
        return -1;
    }
    //需要修改任务 TCB 的内容，所以需要一个非 const 的指针
    Task_t *task_to_delete = (Task_t *) task_h;
    uint32_t primask_status;
    int trigger_yield = 0; // 是否需要在函数末尾触发调度的标志
    MY_RTOS_ENTER_CRITICAL(primask_status);
    //自动释放任务持有的所有互斥锁，防止死锁
    Mutex_t *p_mutex = task_to_delete->held_mutexes_head;
    while (p_mutex != NULL) {
        Mutex_t *next_mutex = p_mutex->next_held_mutex;
        // 手动解锁
        p_mutex->locked = 0;
        p_mutex->owner = (uint32_t) -1;
        p_mutex->owner_tcb = NULL;
        p_mutex->next_held_mutex = NULL;
        // 如果有其他任务在等待这个锁，唤醒
        if (p_mutex->waiting_mask != 0) {
            Task_t *p_task = taskListHead;
            while (p_task != NULL) {
                if (p_task->taskId < 32 && (p_mutex->waiting_mask & (1 << p_task->taskId))) {
                    if (p_task->state == TASK_STATE_BLOCKED) {
                        p_task->state = TASK_STATE_READY;
                        trigger_yield = 1; // 唤醒了任务，进行调度
                    }
                }
                p_task = p_task->next;
            }
        }
        p_mutex = next_mutex;
    }
    //将任务从全局任务链表中移除
    Task_t *prev = NULL;
    Task_t *curr = taskListHead;
    while (curr != NULL && curr != task_to_delete) {
        prev = curr;
        curr = curr->next;
    }
    // 如果任务不在链表中 则直接返回
    if (curr == NULL) {
        MY_RTOS_EXIT_CRITICAL(primask_status);
        return -2;
    }
    if (prev == NULL) {
        taskListHead = curr->next;
    } else {
        prev->next = curr->next;
    }
    //释放任务占用的内存 (栈和TCB)
    rtos_free(curr->stack_base);
    rtos_free(curr);
    //处理调度
    // 如果删除的是当前正在运行的任务，必须立即触发调度
    if (curr == currentTask) {
        currentTask = NULL; // 强制调度器从头开始寻找下一个任务
        trigger_yield = 1;
    }
    MY_RTOS_EXIT_CRITICAL(primask_status);
    // 在临界区之外执行 yield
    if (trigger_yield) {
        MY_RTOS_YIELD();
    }
    DBG_PRINTF("Task %lu deleted and memory reclaimed.\n", task_h->taskId);
    return 0;
}

void Task_Delay(uint32_t tick) {
    if (tick == 0) return;
    uint32_t primask_status;
    MY_RTOS_ENTER_CRITICAL(primask_status);

    currentTask->delay = tick;
    currentTask->state = TASK_STATE_DELAYED;

    MY_RTOS_EXIT_CRITICAL(primask_status);
    MY_RTOS_YIELD();
    __ISB();
}

__attribute__((naked)) void Start_First_Task(void) {
    __asm volatile (
        "ldr r0, =currentTask      \n"
        "ldr r0, [r0]              \n" // r0 = currentTask
        "ldr r0, [r0]              \n" // r0 = currentTask->sp  (指向软件保存区 R4-R11)

        "ldmia r0!, {r4-r11}       \n" // 弹出 R4-R11，r0 指向硬件栈帧
        "msr psp, r0               \n" // PSP = 硬件栈帧起始
        "isb                       \n"

        "movs r0, #2               \n" // Thread+PSP
        "msr control, r0           \n"
        "isb                       \n"

        "ldr r0, =0xFFFFFFFD       \n" // EXC_RETURN: Thread, PSP, Return to Thumb
        "mov lr, r0                \n"
        "bx lr                     \n" // 只能 bx lr
    );
}

void Task_StartScheduler(void) {
    // 创建空闲任务
    idleTask = Task_Create(MyRTOS_Idle_Task, NULL);
    if (idleTask == NULL) {
        DBG_PRINTF("Error: Failed to create Idle Task!\n");
        while (1);
    }
    DBG_PRINTF("Idle task created successfully at address: %p\n", idleTask);
    DBG_PRINTF("Idle task initial SP: %p\n", idleTask->sp);
    SCB->VTOR = (uint32_t) 0x08000000;
    DBG_PRINTF("Starting scheduler...\n");

    //这里要设置PenSV和SysTick的中断优先级 不然会寄
    NVIC_SetPriority(PendSV_IRQn, 0xFF); // 最低优先级
    NVIC_SetPriority(SysTick_IRQn, 0x00); // 比PendSV高一级喵
    if (SysTick_Config(SystemCoreClock / 1000)) {
        // 1ms 嫌不够自己改上面
        DBG_PRINTF("Error: SysTick_Config failed\n");
        while (1);
    }
    DBG_PRINTF("Idle task sp: %p\n", idleTask->sp);
    // 设置当前任务为Idle
    currentTask = idleTask;
    /* 确保 MSP 指向向量表中的初始栈（参考 FreeRTOS 做法） */
    __asm volatile(
        "ldr r0, =0xE000ED08\n"
        "ldr r0, [r0]\n"
        "ldr r0, [r0]\n"
        "msr msp, r0\n"
    );
    /* 触发 SVC 在异常中完成首次上下文恢复 */
    __asm volatile("svc 0");
    for (;;); /* 不会返回 */
    // 理论上不会执行到这里的哈 当然不排除什么宇宙射线导致内存发生比特反转. 这边建议您移步至服务器硬件使用ECC内存
    DBG_PRINTF("Error: Scheduler returned!\n");
    while (1);
}

// 任务调度器核心
void *schedule_next_task(void) {
    Task_t *next_task_to_run = NULL;
    Task_t *start_node;
    // 如果当前任务存在且有后继节点，则从后继节点开始搜索，实现O(1)轮询。
    if (currentTask && currentTask->next) {
        start_node = currentTask->next;
    } else {
        start_node = taskListHead;
    }

    Task_t *p = start_node;
    while (p != NULL) {
        if (p->state == TASK_STATE_READY) {
            next_task_to_run = p;
            goto found_task; // 找到立即跳出
        }
        p = p->next;
    }
    p = taskListHead;
    while (p != start_node) {
        if (p->state == TASK_STATE_READY) {
            next_task_to_run = p;
            goto found_task;
        }
        p = p->next;
    }

found_task:
    if (next_task_to_run != NULL) {
        currentTask = next_task_to_run;
    } else {
        currentTask = idleTask;
    }

    if (currentTask == NULL) {
        return NULL;
    }

    return currentTask->sp;
}

//================= Task ================

//=================== 信号量==================
int Task_Notify(uint32_t task_id) {
    Task_t *task_to_notify = find_task_by_id(task_id);
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

//=================== 信号量==================


//============== 互斥锁 =============
void Mutex_Init(Mutex_t *mutex) {
    mutex->locked = 0;
    mutex->owner = (uint32_t) -1;
    mutex->waiting_mask = 0;
    mutex->owner_tcb = NULL;
    mutex->next_held_mutex = NULL;
}

void Mutex_Lock(Mutex_t *mutex) {
    uint32_t primask_status;

    while (1) {
        // 使用无限循环以简化逻辑
        MY_RTOS_ENTER_CRITICAL(primask_status);
        if (!mutex->locked) {
            // 获取锁成功
            mutex->locked = 1;
            mutex->owner = currentTask->taskId;
            mutex->owner_tcb = currentTask; // 记录持有者TCB
            // --- 将锁添加到任务的持有链表头部
            mutex->next_held_mutex = currentTask->held_mutexes_head;
            currentTask->held_mutexes_head = mutex;
            MY_RTOS_EXIT_CRITICAL(primask_status);
            return; // 成功获取，返回
        } else {
            // 锁被占用，进入等待
            if (currentTask->taskId < 32) {
                mutex->waiting_mask |= (1 << currentTask->taskId);
            }
            currentTask->state = TASK_STATE_BLOCKED;
            // 触发调度，让出CPU
            MY_RTOS_YIELD();
            MY_RTOS_EXIT_CRITICAL(primask_status);
        }
    }
}

void Mutex_Unlock(Mutex_t *mutex) {
    uint32_t primask_status;
    MY_RTOS_ENTER_CRITICAL(primask_status);
    if (mutex->locked && mutex->owner == currentTask->taskId) {
        // --- 从任务的持有链表中移除该锁 ---
        //个简单的单向链表
        if (currentTask->held_mutexes_head == mutex) {
            // 如果是头节点
            currentTask->held_mutexes_head = mutex->next_held_mutex;
        } else {
            Mutex_t *p = currentTask->held_mutexes_head;
            while (p != NULL && p->next_held_mutex != mutex) {
                p = p->next_held_mutex;
            }
            if (p != NULL) {
                p->next_held_mutex = mutex->next_held_mutex;
            }
        }
        mutex->next_held_mutex = NULL; // 清理指针
        // 释放锁
        mutex->locked = 0;
        mutex->owner = (uint32_t) -1;
        mutex->owner_tcb = NULL;
        // 唤醒等待的任务
        if (mutex->waiting_mask != 0) {
            Task_t *p_task = taskListHead;
            while (p_task != NULL) {
                if (p_task->taskId < 32 && (mutex->waiting_mask & (1 << p_task->taskId))) {
                    if (p_task->state == TASK_STATE_BLOCKED) {
                        p_task->state = TASK_STATE_READY;
                    }
                }
                p_task = p_task->next;
            }
            // 唤醒了其他任务，最好进行一次调度
            MY_RTOS_YIELD();
        }
    }
    MY_RTOS_EXIT_CRITICAL(primask_status);
}

//============== 互斥锁 =============


//=========== Handler ============

void SysTick_Handler(void) {
    uint32_t primask_status;
    MY_RTOS_ENTER_CRITICAL(primask_status);

    Task_t *p = taskListHead;
    while (p != NULL) {
        if (p->state == TASK_STATE_DELAYED) {
            if (p->delay > 0) {
                p->delay--;
            }
            if (p->delay == 0) {
                p->state = TASK_STATE_READY;
            }
        }
        p = p->next;
    }
    MY_RTOS_YIELD();
    MY_RTOS_EXIT_CRITICAL(primask_status);
}

__attribute__((naked)) void PendSV_Handler(void) {
    __asm volatile (
        "ldr r2, =currentTask      \n"
        "ldr r3, [r2]              \n"
        "cbz r3, 1f                \n" // currentTask==NULL? 首次，不保存
        "mrs r0, psp               \n"
        "stmdb r0!, {r4-r11}       \n"
        "str  r0, [r3]             \n" // currentTask->sp = 新栈顶
        "1: \n"
        "bl  schedule_next_task    \n" // r0 = next->sp（指向软件保存区底部）
        "ldmia r0!, {r4-r11}       \n"
        "msr psp, r0               \n"
        "mov r0, #0xFFFFFFFD       \n"
        "mov lr, r0                \n"
        "bx  lr                    \n"
    );
}

void HardFault_Handler(void) {
    __disable_irq();

    uint32_t stacked_pc = 0;
    uint32_t sp = 0;
    uint32_t cfsr = SCB->CFSR;
    uint32_t hfsr = SCB->HFSR;

    /* 判断异常返回使用哪条栈 (EXC_RETURN bit2) */
    register uint32_t lr __asm("lr");

    if (lr & 0x4) {
        // 使用 PSP
        sp = __get_PSP();
    } else {
        // 使用 MSP
        sp = __get_MSP();
    }

    stacked_pc = ((uint32_t *) sp)[6]; // PC 存在硬件自动保存帧的第 6 个位置

    DBG_PRINTF("\n!!! Hard Fault !!!\n");
    DBG_PRINTF("CFSR: 0x%08lX, HFSR: 0x%08lX\n", cfsr, hfsr);
    DBG_PRINTF("LR: 0x%08lX, SP: 0x%08lX, Stacked PC: 0x%08lX\n", lr, sp, stacked_pc);

    if (cfsr & SCB_CFSR_IACCVIOL_Msk) DBG_PRINTF("Fault: Instruction Access Violation\n");
    if (cfsr & SCB_CFSR_DACCVIOL_Msk) DBG_PRINTF("Fault: Data Access Violation\n");
    if (cfsr & SCB_CFSR_MUNSTKERR_Msk) DBG_PRINTF("Fault: Unstacking Error\n");
    if (cfsr & SCB_CFSR_MSTKERR_Msk) DBG_PRINTF("Fault: Stacking Error\n");
    if (cfsr & SCB_CFSR_INVSTATE_Msk) DBG_PRINTF("Fault: Invalid State\n");
    if (cfsr & SCB_CFSR_UNDEFINSTR_Msk) DBG_PRINTF("Fault: Undefined Instruction\n");
    if (cfsr & SCB_CFSR_IBUSERR_Msk) DBG_PRINTF("Fault: Instruction Bus Error\n");
    if (cfsr & SCB_CFSR_PRECISERR_Msk) DBG_PRINTF("Fault: Precise Data Bus Error\n");

    while (1);
}


__attribute__((naked)) void SVC_Handler(void) {
    __asm volatile (
        "ldr r1, =currentTask      \n"
        "ldr r1, [r1]              \n" /* r1 = currentTask */
        "ldr r0, [r1]              \n" /* r0 = currentTask->sp (指向软件保存区底部) */

        "ldmia r0!, {r4-r11}       \n" /* 恢复 R4-R11，r0 -> 硬件帧 */
        "msr psp, r0               \n" /* PSP 指向硬件帧 */
        "isb                       \n"

        "movs r0, #2               \n" /* Thread mode, use PSP */
        "msr control, r0           \n"
        "isb                       \n"

        "ldr r0, =0xFFFFFFFD       \n" /* EXC_RETURN: thread, return using PSP */
        "mov lr, r0                \n"
        "bx lr                     \n"
    );
}

//=========== Handler ============
