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
    struct BlockLink_t *nextFreeBlock; /* 指向链表中下一个空闲内存块 */
    size_t blockSize; /* 当前内存块的大小(包含头部), 最高位用作分配标记 */
} BlockLink_t;

/* 内存块头部结构的大小 (已对齐) */
static const size_t heapStructSize = (sizeof(BlockLink_t) + (HEAP_BYTE_ALIGNMENT - 1)) & ~(
                                          (size_t) HEAP_BYTE_ALIGNMENT - 1);
/* 最小内存块大小，一个块必须能容纳分裂后的两个头部 */
#define HEAP_MINIMUM_BLOCK_SIZE    (heapStructSize * 2)

/* 静态内存池 */
static uint8_t rtos_memory_pool[RTOS_MEMORY_POOL_SIZE] __attribute__((aligned(HEAP_BYTE_ALIGNMENT)));
/* 空闲链表的起始和结束标记 */
static BlockLink_t start, *blockLinkEnd = NULL;
/* 剩余可用内存大小 */
static size_t freeBytesRemaining = 0U;
/* 用于标记内存块是否已被分配的标志位 */
static size_t blockAllocatedBit = 0;

/* 初始化堆内存 (仅在首次分配时调用) */
static void heapInit(void) {
    BlockLink_t *firstFreeBlock;
    uint8_t *alignedHeap;
    size_t address = (size_t) rtos_memory_pool;
    size_t totalHeapSize = RTOS_MEMORY_POOL_SIZE;

    if ((address & (HEAP_BYTE_ALIGNMENT - 1)) != 0) {
        address += (HEAP_BYTE_ALIGNMENT - (address & (HEAP_BYTE_ALIGNMENT - 1)));
        totalHeapSize -= address - (size_t) rtos_memory_pool;
    }
    alignedHeap = (uint8_t *) address;

    start.nextFreeBlock = (BlockLink_t *) alignedHeap;
    start.blockSize = (size_t) 0;

    address = ((size_t) alignedHeap) + totalHeapSize - heapStructSize;
    blockLinkEnd = (BlockLink_t *) address;
    blockLinkEnd->blockSize = 0;
    blockLinkEnd->nextFreeBlock = NULL;

    firstFreeBlock = (BlockLink_t *) alignedHeap;
    firstFreeBlock->blockSize = address - (size_t) firstFreeBlock;
    firstFreeBlock->nextFreeBlock = blockLinkEnd;

    freeBytesRemaining = firstFreeBlock->blockSize;
    blockAllocatedBit = ((size_t) 1) << ((sizeof(size_t) * 8) - 1);
}

/* 将一个内存块插入到空闲链表中，并处理与相邻空闲块的合并 */
static void insertBlockIntoFreeList(BlockLink_t *blockToInsert) {
    BlockLink_t *iterator;
    uint8_t *puc;

    /* 遍历链表，找到可以插入新释放块的位置 (按地址排序) */
    for (iterator = &start; iterator->nextFreeBlock < blockToInsert;
         iterator = iterator->nextFreeBlock) {
        /* 什么都不用做，只是找到位置 */
    }

    /* 看看新块是否与迭代器指向的块在物理上相邻 */
    puc = (uint8_t *) iterator;
    if ((puc + iterator->blockSize) == (uint8_t *) blockToInsert) {
        /* 相邻，合并它们 */
        iterator->blockSize += blockToInsert->blockSize;
        /* 合并后，新块就是迭代器指向的块了 */
        blockToInsert = iterator;
    } else {
        /* 不相邻，将新块链接到迭代器后面 */
        blockToInsert->nextFreeBlock = iterator->nextFreeBlock;
    }

    /* 看看新块是否与后面的块相邻 */
    puc = (uint8_t *) blockToInsert;
    if ((puc + blockToInsert->blockSize) == (uint8_t *) iterator->nextFreeBlock) {
        if (iterator->nextFreeBlock != blockLinkEnd) {
            /* 相邻，合并它们 */
            blockToInsert->blockSize += iterator->nextFreeBlock->blockSize;
            blockToInsert->nextFreeBlock = iterator->nextFreeBlock->nextFreeBlock;
        }
    }

    /* 如果第一步没有合并，那么迭代器的下一个节点需要指向新块 */
    if (iterator != blockToInsert) {
        iterator->nextFreeBlock = blockToInsert;
    }
}

/* 动态内存分配函数 */
void *rtos_malloc(const size_t wantedSize) {
    BlockLink_t *block, *previousBlock, *newBlockLink;
    void *pvReturn = NULL;
    uint32_t primask_status;

    MY_RTOS_ENTER_CRITICAL(primask_status); {
        if (blockLinkEnd == NULL) {
            heapInit();
        }

        if ((wantedSize > 0) && ((wantedSize & blockAllocatedBit) == 0)) {
            size_t xTotalSize = heapStructSize + wantedSize;
            if ((xTotalSize & (HEAP_BYTE_ALIGNMENT - 1)) != 0) {
                xTotalSize += (HEAP_BYTE_ALIGNMENT - (xTotalSize & (HEAP_BYTE_ALIGNMENT - 1)));
            }

            if (xTotalSize <= freeBytesRemaining) {
                previousBlock = &start;
                block = start.nextFreeBlock;
                while ((block->blockSize < xTotalSize) && (block->nextFreeBlock != NULL)) {
                    previousBlock = block;
                    block = block->nextFreeBlock;
                }

                if (block != blockLinkEnd) {
                    pvReturn = (void *) (((uint8_t *) block) + heapStructSize);
                    previousBlock->nextFreeBlock = block->nextFreeBlock;

                    if ((block->blockSize - xTotalSize) > HEAP_MINIMUM_BLOCK_SIZE) {
                        newBlockLink = (BlockLink_t *) (((uint8_t *) block) + xTotalSize);
                        newBlockLink->blockSize = block->blockSize - xTotalSize;
                        block->blockSize = xTotalSize;
                        insertBlockIntoFreeList(newBlockLink);
                    }

                    freeBytesRemaining -= block->blockSize;
                    block->blockSize |= blockAllocatedBit;
                    block->nextFreeBlock = NULL;
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
    BlockLink_t *link;
    uint32_t primask_status;

    puc -= heapStructSize;
    link = (BlockLink_t *) puc;

    if (((link->blockSize & blockAllocatedBit) != 0) && (link->nextFreeBlock == NULL)) {
        link->blockSize &= ~blockAllocatedBit;
        MY_RTOS_ENTER_CRITICAL(primask_status); {
            freeBytesRemaining += link->blockSize;
            insertBlockIntoFreeList(link);
        }
        MY_RTOS_EXIT_CRITICAL(primask_status);
    }
}

//====================== 动态内存管理 ======================


//================= Task ================


static Task_t *allTaskListHead = NULL;
static Task_t *currentTask = NULL;
static Task_t *idleTask = NULL;
static uint32_t nextTaskId = 0;

static Task_t *readyTaskLists[MY_RTOS_MAX_PRIORITIES];
// 延时任务列表，按唤醒时间排序（不排序，仅作列表）
static Task_t *delayedTaskListHead = NULL;
// 用于快速查找最高优先级就绪任务的bitmap
static volatile uint32_t topReadyPriority = 0;

#define SIZEOF_TASK_T sizeof(Task_t)


void MyRTOS_Init(void) {
    allTaskListHead = NULL;
    currentTask = NULL;
    idleTask = NULL;
    nextTaskId = 0;
    for (int i = 0; i < MY_RTOS_MAX_PRIORITIES; i++) {
        readyTaskLists[i] = NULL;
    }
    delayedTaskListHead = NULL;
    topReadyPriority = 0;
    DBG_PRINTF("MyRTOS Initialized. Task list cleared and memory manager reset.\n");
}

static Task_t *find_task_by_id(uint32_t task_id) {
    Task_t *p = allTaskListHead;
    while (p != NULL) {
        if (p->taskId == task_id) return p;
        p = p->pNextTask;
    }
    return NULL;
}

static void addTaskToReadyList(Task_t *task) {
    if (task == NULL || task->priority >= MY_RTOS_MAX_PRIORITIES) {
        return;
    }

    uint32_t primask_status;
    MY_RTOS_ENTER_CRITICAL(primask_status);

    // 标记该优先级有就绪任务了
    topReadyPriority |= (1UL << task->priority);

    // 将任务插入到对应优先级链表的尾部
    task->pNextReady = NULL;
    if (readyTaskLists[task->priority] == NULL) {
        // 这是该优先级的第一个任务
        readyTaskLists[task->priority] = task;
        task->pPrevReady = NULL;
    } else {
        // 找到链表尾部并插入
        Task_t *pLast = readyTaskLists[task->priority];
        while (pLast->pNextReady != NULL) {
            pLast = pLast->pNextReady;
        }
        pLast->pNextReady = task;
        task->pPrevReady = pLast;
    }
    task->state = TASK_STATE_READY;

    MY_RTOS_EXIT_CRITICAL(primask_status);
}

static void removeTaskFromList(Task_t **ppListHead, Task_t *pTaskToRemove) {
    if (pTaskToRemove == NULL) return;

    // 更新前一个节点的 next 指针
    if (pTaskToRemove->pPrevReady != NULL) {
        pTaskToRemove->pPrevReady->pNextReady = pTaskToRemove->pNextReady;
    } else {
        // 移除的是头节点
        *ppListHead = pTaskToRemove->pNextReady;
    }

    // 更新后一个节点的 prev 指针
    if (pTaskToRemove->pNextReady != NULL) {
        pTaskToRemove->pNextReady->pPrevReady = pTaskToRemove->pPrevReady;
    }

    // 清理被移除节点的指针
    pTaskToRemove->pNextReady = NULL;
    pTaskToRemove->pPrevReady = NULL;

    // 如果任务是从就绪列表中移除，还需要检查是否需要清除优先级位图
    if (pTaskToRemove->state == TASK_STATE_READY) {
        if (readyTaskLists[pTaskToRemove->priority] == NULL) {
            topReadyPriority &= ~(1UL << pTaskToRemove->priority);
        }
    }
}


void MyRTOS_Idle_Task(void *pv) {
    DBG_PRINTF("Idle Task starting\n");
    while (1) {
        // DBG_PRINTF("Idle task running...\n");
        __WFI(); // 进入低功耗模式，等待中断
    }
}

Task_t *Task_Create(void (*func)(void *), void *param, uint8_t priority) {
    // 检查优先级是否有效
    if (priority >= MY_RTOS_MAX_PRIORITIES) {
        DBG_PRINTF("Error: Invalid task priority %u.\n", priority);
        return NULL;
    }

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
        rtos_free(t);
        return NULL;
    }

    // 初始化TCB成员
    t->func = func;
    t->param = param;
    t->delay = 0;
    t->notification = 0;
    t->is_waiting_notification = 0;
    // t->state 在 prvAddTaskToReadyList 中设置
    t->taskId = nextTaskId++;
    t->stack_base = stack;
    t->pNextTask = NULL;
    t->pNextReady = NULL;
    t->pPrevReady = NULL;
    t->priority = priority;

    uint32_t *sp = stack + STACK_SIZE;
    sp = (uint32_t *) (((uintptr_t) sp) & ~0x7u);
    sp -= 8;
    sp[0] = (uint32_t) param;
    sp[1] = 0x01010101;
    sp[2] = 0x02020202;
    sp[3] = 0x03030303;
    sp[4] = 0x12121212;
    sp[5] = 0x00000000;
    sp[6] = ((uint32_t) func) | 1u;
    sp[7] = 0x01000000;
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

    // 添加到所有任务链表
    if (allTaskListHead == NULL) {
        allTaskListHead = t;
    } else {
        Task_t *p = allTaskListHead;
        while (p->pNextTask != NULL) {
            p = p->pNextTask;
        }
        p->pNextTask = t;
    }

    addTaskToReadyList(t);

    MY_RTOS_EXIT_CRITICAL(primask_status);

    DBG_PRINTF("Task %lu created with priority %u. Stack top: %p, Initial SP: %p\n", t->taskId, t->priority,
               &stack[STACK_SIZE - 1], t->sp);
    return t;
}

// 删除任务
int Task_Delete(const Task_t *task_h) {
    // 不允许删除 NULL 任务或空闲任务
    if (task_h == idleTask) {
        return -1;
    }
    //删除当前任务
    if (task_h == NULL) {
        task_h = currentTask;
    }
    // 需要修改任务 TCB 的内容，所以需要一个非 const 的指针
    Task_t *task_to_delete = (Task_t *) task_h;
    uint32_t primask_status;
    int trigger_yield = 0; // 是否需要在函数末尾触发调度的标志

    MY_RTOS_ENTER_CRITICAL(primask_status);

    //根据任务当前状态，从对应的状态链表中移除
    switch (task_to_delete->state) {
        case TASK_STATE_READY:
            removeTaskFromList(&readyTaskLists[task_to_delete->priority], task_to_delete);
            break;
        case TASK_STATE_DELAYED:
            removeTaskFromList(&delayedTaskListHead, task_to_delete);
            break;
        case TASK_STATE_BLOCKED:
            // 阻塞状态的任务不在任何活动列表中，无需操作
            break;
        default:
            // 其他状态或无效状态
            break;
    }

    // 将任务状态标记为未使用，防止其他地方误操作
    task_to_delete->state = TASK_STATE_UNUSED;

    //自动释放任务持有的所有互斥锁，防止死锁
    Mutex_t *p_mutex = task_to_delete->held_mutexes_head;
    while (p_mutex != NULL) {
        Mutex_t *next_mutex = p_mutex->next_held_mutex;

        // 手动解锁
        p_mutex->locked = 0;
        p_mutex->owner = (uint32_t) -1;
        p_mutex->owner_tcb = NULL;
        p_mutex->next_held_mutex = NULL; // 从持有链中断开

        // 如果有其他任务在等待这个锁，唤醒它们
        if (p_mutex->waiting_mask != 0) {
            Task_t *p_task = allTaskListHead; // 遍历所有任务来查找等待者
            while (p_task != NULL) {
                if (p_task->taskId < 32 && (p_mutex->waiting_mask & (1 << p_task->taskId))) {
                    if (p_task->state == TASK_STATE_BLOCKED) {
                        //唤醒任务
                        p_mutex->waiting_mask &= ~(1UL << p_task->taskId); // 清除等待标志
                        addTaskToReadyList(p_task); // 将任务放回就绪列表
                        // 检查是否需要抢占
                        if (p_task->priority > currentTask->priority) {
                            trigger_yield = 1;
                        }
                    }
                }
                p_task = p_task->pNextTask;
            }
        }
        p_mutex = next_mutex;
    }

    Task_t *prev = NULL;
    Task_t *curr = allTaskListHead;
    while (curr != NULL && curr != task_to_delete) {
        prev = curr;
        curr = curr->pNextTask; // <<< 修改点: 使用 pNextTask 指针
    }

    if (curr == NULL) {
        // 任务不在管理链表中，可能已损坏，直接返回
        MY_RTOS_EXIT_CRITICAL(primask_status);
        return -2;
    }

    if (prev == NULL) {
        allTaskListHead = curr->pNextTask;
    } else {
        prev->pNextTask = curr->pNextTask;
    }

    //如果删除的是当前正在运行的任务，必须立即触发调度
    if (curr == currentTask) {
        trigger_yield = 1;
    }

    rtos_free(curr->stack_base);
    rtos_free(curr);

    MY_RTOS_EXIT_CRITICAL(primask_status);

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

    //从就绪列表中移除当前任务
    removeTaskFromList(&readyTaskLists[currentTask->priority], currentTask);

    currentTask->delay = tick;
    currentTask->state = TASK_STATE_DELAYED;

    //简化: 头插法
    currentTask->pNextReady = delayedTaskListHead;
    currentTask->pPrevReady = NULL;
    if (delayedTaskListHead != NULL) {
        delayedTaskListHead->pPrevReady = currentTask;
    }
    delayedTaskListHead = currentTask;

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

// 任务调度器核心
void *schedule_next_task(void) {
    Task_t *nextTaskToRun = NULL;

    if (topReadyPriority == 0) {
        // 没有就绪任务，只能运行空闲任务
        nextTaskToRun = idleTask;
    } else {
        // AI给的魔法操作: 使用 CLZ (Count Leading Zeros) 指令快速找到最高的置位
        // 31 - __CLZ(topReadyPriority) 能直接得到最高位的索引
        uint32_t highestPriority = 31 - __CLZ(topReadyPriority);

        // 从该优先级的就绪链表头取出任务
        nextTaskToRun = readyTaskLists[highestPriority];

        // 实现同优先级任务的轮询 (Round-Robin)
        // 如果该优先级有多个任务，则将当前选中的任务移到链表尾部
        if (nextTaskToRun != NULL && nextTaskToRun->pNextReady != NULL) {
            readyTaskLists[highestPriority] = nextTaskToRun->pNextReady;
            nextTaskToRun->pNextReady->pPrevReady = NULL;

            Task_t *pLast = readyTaskLists[highestPriority];
            while (pLast->pNextReady != NULL) {
                pLast = pLast->pNextReady;
            }
            pLast->pNextReady = nextTaskToRun;
            nextTaskToRun->pPrevReady = pLast;
            nextTaskToRun->pNextReady = NULL;
        }
    }

    currentTask = nextTaskToRun;

    if (currentTask == NULL) {
        // 不应该哈
        return NULL;
    }

    return currentTask->sp;
}

void Task_StartScheduler(void) {
    // 创建空闲任务 优先级最低 (0)
    idleTask = Task_Create(MyRTOS_Idle_Task, NULL, 0);
    if (idleTask == NULL) {
        DBG_PRINTF("Error: Failed to create Idle Task!\n");
        while (1);
    }

    SCB->VTOR = (uint32_t) 0x08000000;
    DBG_PRINTF("Starting scheduler...\n");

    NVIC_SetPriority(PendSV_IRQn, 0xFF);
    NVIC_SetPriority(SysTick_IRQn, 0x00);
    if (SysTick_Config(SystemCoreClock / 1000)) {
        DBG_PRINTF("Error: SysTick_Config failed\n");
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
    __asm volatile("svc 0");
    for (;;);
}


//================= Task ================

//=================== 信号量==================
int Task_Notify(uint32_t task_id) {
    // 查找任务可以在临界区之外进行，提高并发性
    Task_t *task_to_notify = find_task_by_id(task_id);
    if (task_to_notify == NULL) {
        return -1; // 无效ID
    }

    uint32_t primask_status;
    int trigger_yield = 0; // 是否需要抢占调度的标志

    MY_RTOS_ENTER_CRITICAL(primask_status);

    // 检查任务是否确实在等待通知
    if (task_to_notify->is_waiting_notification && task_to_notify->state == TASK_STATE_BLOCKED) {
        // 清除等待标志
        task_to_notify->is_waiting_notification = 0;

        //将任务重新添加到就绪列表中，使其可以被调度
        addTaskToReadyList(task_to_notify);

        //检查是否需要抢占：如果被唤醒的任务优先级高于当前任务
        if (task_to_notify->priority > currentTask->priority) {
            trigger_yield = 1;
        }
    }

    MY_RTOS_EXIT_CRITICAL(primask_status);

    if (trigger_yield) {
        MY_RTOS_YIELD();
    }

    return 0;
}

void Task_Wait(void) {
    uint32_t primask_status;
    MY_RTOS_ENTER_CRITICAL(primask_status);

    removeTaskFromList(&readyTaskLists[currentTask->priority], currentTask);

    currentTask->is_waiting_notification = 1;
    currentTask->state = TASK_STATE_BLOCKED;

    MY_RTOS_YIELD();

    MY_RTOS_EXIT_CRITICAL(primask_status);

    // __ISB 确保流水线被刷新
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
        MY_RTOS_ENTER_CRITICAL(primask_status);
        if (!mutex->locked) {
            mutex->locked = 1;
            mutex->owner = currentTask->taskId;
            mutex->owner_tcb = currentTask;
            mutex->next_held_mutex = currentTask->held_mutexes_head;
            currentTask->held_mutexes_head = mutex;
            MY_RTOS_EXIT_CRITICAL(primask_status);
            return;
        }
        // 锁被占用,进入等待
        if (currentTask->taskId < 32) {
            mutex->waiting_mask |= (1 << currentTask->taskId);
        }
        // 从就绪列表移除
        removeTaskFromList(&readyTaskLists[currentTask->priority], currentTask);
        currentTask->state = TASK_STATE_BLOCKED;

        // 触发调度
        MY_RTOS_YIELD();
        MY_RTOS_EXIT_CRITICAL(primask_status);
        // 当任务被唤醒后，会从这里继续执行
    }
}

void Mutex_Unlock(Mutex_t *mutex) {
    uint32_t primask_status;
    int trigger_yield = 0;

    MY_RTOS_ENTER_CRITICAL(primask_status);
    if (mutex->locked && mutex->owner == currentTask->taskId) {
        if (currentTask->held_mutexes_head == mutex) {
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
        mutex->next_held_mutex = NULL;

        // 释放锁
        mutex->locked = 0;
        mutex->owner = (uint32_t) -1;
        mutex->owner_tcb = NULL;

        // 唤醒等待的任务
        if (mutex->waiting_mask != 0) {
            Task_t *p_task = allTaskListHead; // <<< 遍历所有任务
            while (p_task != NULL) {
                if (p_task->taskId < 32 && (mutex->waiting_mask & (1 << p_task->taskId))) {
                    if (p_task->state == TASK_STATE_BLOCKED) {
                        // 清除等待标志
                        mutex->waiting_mask &= ~(1 << p_task->taskId);
                        // 添加回就绪列表
                        addTaskToReadyList(p_task);
                        // 如果被唤醒任务的优先级高于当前任务，则需要调度
                        if (p_task->priority > currentTask->priority) {
                            trigger_yield = 1;
                        }
                    }
                }
                p_task = p_task->pNextTask;
            }
        }

        if (trigger_yield) {
            MY_RTOS_YIELD();
        }
    }
    MY_RTOS_EXIT_CRITICAL(primask_status);
}

//============== 互斥锁 =============


//====================== 消息队列 ======================
static void addTaskToPrioritySortedList(Task_t **listHead, Task_t *taskToInsert) {
    if (*listHead == NULL || (*listHead)->priority <= taskToInsert->priority) {
        // 插入到头部 (列表为空或新任务优先级更高或相等)
        taskToInsert->pNextEvent = *listHead;
        *listHead = taskToInsert;
    } else {
        // 遍历查找插入点
        Task_t *iterator = *listHead;
        while (iterator->pNextEvent != NULL && iterator->pNextEvent->priority > taskToInsert->priority) {
            iterator = iterator->pNextEvent;
        }
        taskToInsert->pNextEvent = iterator->pNextEvent;
        iterator->pNextEvent = taskToInsert;
    }
}

QueueHandle_t Queue_Create(uint32_t length, uint32_t itemSize) {
    if (length == 0 || itemSize == 0) {
        return NULL;
    }

    Queue_t *queue = rtos_malloc(sizeof(Queue_t));
    if (queue == NULL) {
        return NULL;
    }

    queue->storage = (uint8_t *)rtos_malloc(length * itemSize);
    if (queue->storage == NULL) {
        rtos_free(queue);
        return NULL;
    }
    queue->length = length;
    queue->itemSize = itemSize;
    queue->waitingCount = 0;
    queue->writePtr = queue->storage;
    queue->readPtr = queue->storage;
    queue->sendWaitList = NULL;
    queue->receiveWaitList = NULL;
    return queue;
}

void Queue_Delete(QueueHandle_t delQueue) {
    Queue_t* queue = delQueue;
    if (queue == NULL) return;
    uint32_t primask_status;
    MY_RTOS_ENTER_CRITICAL(primask_status);
    // 唤醒所有等待的任务 (它们将从 Send/Receive 调用中失败返回)
    while(queue->sendWaitList) {
        Task_t* taskToWake = queue->sendWaitList;
        queue->sendWaitList = taskToWake->pNextEvent;
        addTaskToReadyList(taskToWake);
    }
    while(queue->receiveWaitList) {
        Task_t* taskToWake = queue->receiveWaitList;
        queue->receiveWaitList = taskToWake->pNextEvent;
        addTaskToReadyList(taskToWake);
    }
    rtos_free(queue->storage);
    rtos_free(queue);
    MY_RTOS_EXIT_CRITICAL(primask_status);
}

int Queue_Send(QueueHandle_t queue, const void *item, int block) {
    Queue_t* pQueue = queue;
    if (pQueue == NULL) return 0;
    uint32_t primask_status;
    while(1) { // 循环用于阻塞后重试
        MY_RTOS_ENTER_CRITICAL(primask_status);
        // 优先级拦截：检查是否有高优先级任务在等待接收
        if (pQueue->receiveWaitList != NULL) {
            // 直接将数据传递给等待的最高优先级任务，不经过队列存储区
            Task_t *taskToWake = pQueue->receiveWaitList;
            pQueue->receiveWaitList = taskToWake->pNextEvent; // 从等待列表移除
            memcpy(taskToWake->eventData, item, pQueue->itemSize);
            // 唤醒该任务
            taskToWake->eventObject = NULL;
            taskToWake->eventData = NULL;
            addTaskToReadyList(taskToWake);
            // 如果被唤醒的任务优先级更高，触发调度
            if (taskToWake->priority > currentTask->priority) {
                MY_RTOS_YIELD();
            }
            MY_RTOS_EXIT_CRITICAL(primask_status);
            return 1; // 发送成功
        }
        // 没有任务在等待，尝试放入队列缓冲区
        if (pQueue->waitingCount < pQueue->length) {
            memcpy(pQueue->writePtr, item, pQueue->itemSize);
            pQueue->writePtr += pQueue->itemSize;
            if (pQueue->writePtr >= (pQueue->storage + (pQueue->length * pQueue->itemSize))) {
                pQueue->writePtr = pQueue->storage;
            }
            pQueue->waitingCount++;
            MY_RTOS_EXIT_CRITICAL(primask_status);
            return 1; // 发送成功
        }
        // 队列已满
        if (block == 0) {
            MY_RTOS_EXIT_CRITICAL(primask_status);
            return 0; // 不阻塞，直接返回失败
        }
        // 阻塞当前任务
        currentTask->eventObject = pQueue;
        addTaskToPrioritySortedList(&pQueue->sendWaitList, currentTask);
        removeTaskFromList(&readyTaskLists[currentTask->priority], currentTask);
        currentTask->state = TASK_STATE_BLOCKED;
        MY_RTOS_YIELD();
        MY_RTOS_EXIT_CRITICAL(primask_status);
        // 被唤醒后，将回到 while(1) 循环的开始，重新尝试发送
    }
}

int Queue_Receive(QueueHandle_t queue, void *buffer, int block) {
    Queue_t* pQueue = queue;
    if (pQueue == NULL) return 0;
    uint32_t primask_status;
    while(1) { // 循环用于阻塞后重试
        MY_RTOS_ENTER_CRITICAL(primask_status);
        if (pQueue->waitingCount > 0) {
            // 从队列缓冲区读取数据
            memcpy(buffer, pQueue->readPtr, pQueue->itemSize);
            pQueue->readPtr += pQueue->itemSize;
            if (pQueue->readPtr >= (pQueue->storage + (pQueue->length * pQueue->itemSize))) {
                pQueue->readPtr = pQueue->storage;
            }
            pQueue->waitingCount--;
            // 检查是否有任务在等待发送 (唤醒最高优先级的)
            if (pQueue->sendWaitList != NULL) {
                Task_t *taskToWake = pQueue->sendWaitList;
                pQueue->sendWaitList = taskToWake->pNextEvent;

                // 唤醒它，让它自己去发送
                taskToWake->eventObject = NULL;
                addTaskToReadyList(taskToWake);

                // 如果被唤醒的任务优先级更高，触发调度
                if (taskToWake->priority > currentTask->priority) {
                    MY_RTOS_YIELD();
                }
            }

            MY_RTOS_EXIT_CRITICAL(primask_status);
            return 1; // 接收成功
        }

        // 队列为空
        if (block == 0) {
            MY_RTOS_EXIT_CRITICAL(primask_status);
            return 0; // 不阻塞，直接返回失败
        }

        // 阻塞当前任务
        currentTask->eventObject = pQueue;
        currentTask->eventData = buffer; // 保存接收缓冲区的地址！
        addTaskToPrioritySortedList(&pQueue->receiveWaitList, currentTask);
        removeTaskFromList(&readyTaskLists[currentTask->priority], currentTask);
        currentTask->state = TASK_STATE_BLOCKED;

        MY_RTOS_YIELD();
        MY_RTOS_EXIT_CRITICAL(primask_status);

        // 被唤醒后，有两种可能：
        // 1. 被Queue_Send直接传递了数据 -> pEventObject会是NULL，任务完成
        // 2. 被Queue_Delete唤醒 -> pEventObject可能不是NULL，循环会失败
        if(currentTask->eventObject == NULL) {
             // 成功被 Queue_Send 唤醒并接收到数据
             return 1;
        }
        // 否则，回到循环顶部重试
    }
}



//=========== Handler ============

void SysTick_Handler(void) {
    uint32_t primask_status;
    MY_RTOS_ENTER_CRITICAL(primask_status);

    Task_t *p = delayedTaskListHead;
    Task_t *pNext = NULL;

    // 遍历延时链表
    while (p != NULL) {
        pNext = p->pNextReady; // 先保存下一个节点，因为当前节点可能被移除

        if (p->delay > 0) {
            p->delay--;
        }

        if (p->delay == 0) {
            // 延时结束，将任务移回就绪列表
            removeTaskFromList(&delayedTaskListHead, p);
            addTaskToReadyList(p);
        }

        p = pNext;
    }

    // 检查是否有更高优先级的任务已经就绪，如果是，则需要调度
    //简化 每次SysTick都请求调度
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
