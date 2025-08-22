//
// Created by XiaoXiu on 8/22/2025.
//

#include "MyRTOS.h"

#include <stdint.h>
#include <stdio.h>

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

#define SIZEOF_TASK_T sizeof(Task_t)

// 全局变量
static Task_t tasks[MAX_TASKS] __attribute__((section(".tasks"))) = {{0}};
static uint32_t stacks[MAX_TASKS][STACK_SIZE] __attribute__((section(".stacks"), aligned(8)));
static uint32_t currentTaskId = IDLE_TASK_ID; // 初始时指向一个无效或idle任务ID

// 初始化RTOS，清空所有任务槽
void MyRTOS_Init(void) {
    for (int i = 0; i < MAX_TASKS; i++) {
        tasks[i].state = TASK_STATE_UNUSED;
    }
    DBG_PRINTF("MyRTOS Initialized. Task slots cleared.\n");
}

void MyRTOS_Idle_Task(void *pv) {
    DBG_PRINTF("Idle Task starting\n");
    while (1) {
        // DBG_PRINTF("Idle task running...\n");
        __WFI(); // 进入低功耗模式，等待中断
    }
}

int Task_Create(uint32_t taskId, void (*func)(void *), void *param) {
    if (taskId >= MAX_TASKS) {
        DBG_PRINTF("Error: Task ID %ld is out of bounds.\n", taskId);
        return -1; // ID 无效
    }
    if (tasks[taskId].state != TASK_STATE_UNUSED) {
        DBG_PRINTF("Error: Task ID %ld is already in use.\n", taskId);
        return -2; // ID 已被占用
    }

    Task_t *t = &tasks[taskId];
    t->func = func;
    t->param = param;
    t->delay = 0;
    t->notification = 0;
    t->is_waiting_notification = 0;
    t->state = TASK_STATE_READY;

    // 栈初始化 (与之前相同)
    uint32_t *stk = &stacks[taskId][STACK_SIZE];
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
    DBG_PRINTF("Task %ld created. Stack top: %p, Initial SP: %p\n", taskId, &stacks[taskId][STACK_SIZE - 1], t->sp);
    return 0; // 创建成功
}

void Task_Delay(uint32_t tick) {
    if (tick == 0) return;
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    tasks[currentTaskId].delay = tick;
    tasks[currentTaskId].state = TASK_STATE_DELAYED;
    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk; // 触发调度

    __set_PRIMASK(primask);
    __ISB();
}

void *schedule_next_task(void) {
    uint32_t next_task_id = currentTaskId;
    // 从下一个任务ID开始，轮询查找就绪任务
    for (int i = 1; i <= MAX_TASKS; i++) {
        next_task_id = (currentTaskId + i) % MAX_TASKS;
        if (tasks[next_task_id].state == TASK_STATE_READY) {
            currentTaskId = next_task_id;
            DBG_PRINTF("Scheduler: Switching to task %ld\n", currentTaskId);
            return tasks[currentTaskId].sp;
        }
    }
    // 理论上不会到这里
    currentTaskId = IDLE_TASK_ID;
    return tasks[currentTaskId].sp;
}


void SysTick_Handler(void) {
    uint32_t ulPreviousMask = __get_PRIMASK();
    __disable_irq();

    for (uint32_t i = 0; i < MAX_TASKS; i++) {
        // 只处理处于延时状态的任务
        if (tasks[i].state == TASK_STATE_DELAYED) {
            if (tasks[i].delay > 0) {
                tasks[i].delay--;
            }
            if (tasks[i].delay == 0) {
                // 延时结束，任务恢复为就绪态
                tasks[i].state = TASK_STATE_READY;
            }
        }
    }

    // 手动触发PendSV进行调度，让就绪任务有机会被调用
    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
    __set_PRIMASK(ulPreviousMask);
}

__attribute__((naked)) void PendSV_Handler(void) {
    __asm volatile (
        "mrs r0, psp                   \n" // 获取当前任务的栈指针
        "stmdb r0!, {r4-r11}           \n" // 保存 R4-R11 到任务栈

        "ldr r1, =currentTaskId        \n" // 加载 currentTaskId 的地址
        "ldr r2, [r1]                  \n" // 获取 currentTaskId 的值
        "ldr r3, =tasks                \n" // 加载 tasks 数组的基地址
        "mov r4, %[task_size]          \n" // r4 = sizeof(Task_t)
        "mul r2, r2, r4                \n" // r2 = currentTaskId * sizeof(Task_t)
        "add r3, r3, r2                \n" // r3 = &tasks[currentTaskId]
        "str r0, [r3]                  \n" // 保存新的栈顶到 tasks[currentTaskId].sp (sp是第一个成员，偏移为0)

        "push {lr}                     \n" // 保存 LR
        "bl schedule_next_task         \n" // 调用调度器获取下一个任务的SP，返回值在R0
        "pop {lr}                      \n"

        "ldmia r0!, {r4-r11}           \n" // 从新任务的栈中恢复 R4-R11
        "msr psp, r0                   \n" // 更新 PSP
        "bx lr                         \n"
        : : [task_size] "i" (SIZEOF_TASK_T) // SIZEOF_TASK_T 传入汇编
    );
}

__attribute__((naked)) void Start_First_Task(void) {
    __asm volatile (
        //获取第一个任务的栈顶指针
        "ldr r0, =currentTaskId        \n"
        "ldr r1, [r0]                  \n" // r1 = currentTaskId
        "ldr r2, =tasks                \n" // r2 = &tasks
        "mov r3, %[task_size]          \n"
        "mul r1, r1, r3                \n"
        "add r2, r2, r1                \n" // r2 = &tasks[currentTaskId]
        "ldr r0, [r2]                  \n" // r0 = tasks[currentTaskId].sp

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
        : : [task_size] "i" (sizeof(Task_t))
    );
}


void Task_StartScheduler(void) {
    // 创建空闲任务
    if (Task_Create(IDLE_TASK_ID, MyRTOS_Idle_Task, NULL) != 0) {
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
    currentTaskId = IDLE_TASK_ID;
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
    if (cfsr & SCB_CFSR_INVSTATE_Msk) DBG_PRINTF("Fault: Invalid State\n");
    if (cfsr & SCB_CFSR_UNDEFINSTR_Msk) DBG_PRINTF("Fault: Undefined Instruction\n");
    if (cfsr & SCB_CFSR_IBUSERR_Msk) DBG_PRINTF("Fault: Instruction Bus Error\n");
    if (cfsr & SCB_CFSR_PRECISERR_Msk) DBG_PRINTF("Fault: Precise Data Bus Error\n");
    while (1);
}

//=================互斥锁========================

void Mutex_Init(Mutex_t *mutex) {
    mutex->locked = 0;
    mutex->owner = (uint32_t) -1;
    mutex->waiting_mask = 0;
}

void Mutex_Lock(Mutex_t *mutex) {
    uint32_t primask;
    primask = __get_PRIMASK(); 
    __disable_irq(); //这边就是进入临界区

    while (mutex->locked && mutex->owner != currentTaskId) {
        // 锁被其他任务持有，当前任务需要等待
        mutex->waiting_mask |= (1 << currentTaskId);
        tasks[currentTaskId].state = TASK_STATE_BLOCKED; // 标记为阻塞

        __set_PRIMASK(primask); // 允许中断以进行调度
        SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
        __ISB();
        __WFI(); // 等待被唤醒

        // 唤醒后，重新关闭中断并再次检查锁
        primask = __get_PRIMASK();
        __disable_irq();
    }

    // 获取锁成功
    mutex->locked = 1;
    mutex->owner = currentTaskId;
    mutex->waiting_mask &= ~(1 << currentTaskId); // 从等待掩码中移除自己
    __set_PRIMASK(primask); // 退出临界区
}

void Mutex_Unlock(Mutex_t *mutex) {
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    if (mutex->locked && mutex->owner == currentTaskId) {
        mutex->locked = 0;
        mutex->owner = (uint32_t) -1;

        if (mutex->waiting_mask != 0) {
            // 有任务在等待，唤醒一个
            for (uint32_t i = 0; i < MAX_TASKS; i++) {
                if (mutex->waiting_mask & (1 << i)) {
                    if (tasks[i].state == TASK_STATE_BLOCKED) {
                        tasks[i].state = TASK_STATE_READY; // 唤醒任务
                        // 不再传递所有权，让被唤醒的任务自己竞争，这更符合常规OS行为
                        // mutex->waiting_mask &= ~(1 << i); // 让Lock函数自己处理
                    }
                }
            }
            // 触发调度
            SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
        }
    }

    __set_PRIMASK(primask);
}

//======================挂起 通知==================

int Task_Notify(uint32_t task_id) {
    if (task_id >= MAX_TASKS || tasks[task_id].state == TASK_STATE_UNUSED) {
        return -1; // 无效ID
    }
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    if (tasks[task_id].is_waiting_notification && tasks[task_id].state == TASK_STATE_BLOCKED) {
        tasks[task_id].is_waiting_notification = 0;
        tasks[task_id].state = TASK_STATE_READY; // 唤醒
        //手动触发,通知能更快处理
        SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
    }
    __set_PRIMASK(primask);
    return 0;
}

void Task_Wait(void) {
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    tasks[currentTaskId].is_waiting_notification = 1;
    tasks[currentTaskId].state = TASK_STATE_BLOCKED; // 设置为阻塞
    __set_PRIMASK(primask);
    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk; // 触发调度
    __ISB();
}
