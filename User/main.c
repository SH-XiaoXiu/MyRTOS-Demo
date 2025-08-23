#include <stdint.h>
#include <stdio.h>

#include "lib_usart0.h"
#include "MyRTOS.h"

#define A_TASK 0
#define B_TASK 1
#define C_TASK 2

Mutex_t print_lock;

Task_t *a_task_h;
Task_t *b_task_h;
Task_t *c_task_h;

void a_task(void *param) {
    static uint8_t i = 0;
    Mutex_Lock(&print_lock); //为了启动时打印混乱 加锁
    printf("任务 A 启动\n");
    Mutex_Unlock(&print_lock);
    while (1) {
        Task_Wait();
        while (1) {
            printf("A 任务正在运行,第 %d 次\n", i);
            i++;
            if (i == 5) {
                i = 0;
                printf("任务 A 唤醒 任务 B, 并开始等待 任务 B 的唤醒\n");
                Task_Notify(b_task_h->taskId);
                break;
            }
            Task_Delay(1000); //1s
        }
    }
}


void b_task(void *param) {
    static uint8_t i = 0;
    Mutex_Lock(&print_lock);
    printf("任务 B 启动\n");
    Mutex_Unlock(&print_lock);
    while (1) {
        Task_Wait();
        while (1) {
            printf("B 任务正在运行,第 %d 次\n", i);
            i++;
            if (i == 3) {
                i = 0;
                printf("任务 B 唤醒 任务 A, 并开始等待 任务 A 的唤醒\n");
                Task_Notify(a_task_h->taskId);
                break;
            }
            Task_Delay(1000); //1s
        }
    }
}

void c_task(void *param) {
    uint32_t index = 0;
    Mutex_Lock(&print_lock);
    printf("任务 C 启动\n");
    printf("任务 C 唤醒 任务 A\n");
    Mutex_Unlock(&print_lock);
    Task_Notify(a_task_h->taskId);
    while (1) {
        index++;
        printf("任务 C 正在运行,第 %d 次\n", index);
        Task_Delay(2000);
    }
}


void sys_config() {
    lib_usart0_init();
    // 初始化互斥锁
    Mutex_Init(&print_lock);
}


/**
 * @brief 一个ARM32的RTOS简易实现
 * @author A_XiaoXiu
 * @remark 参考如下
 * \n
 * FreeRTOS: https://github.com/FreeRTOS/FreeRTOS-Kernel
 * \n
 * AI: https://chatgpt.com/ -> PSP汇编部分
 * 最简易实现, 无任务删除, 无任务优先级
 * 无优化策略
 * @return void
 */
int main(void) {
    sys_config();
    printf("=========   =========================\n");
    printf("|        RTOS Demo          \n");
    printf("|  Author: XiaoXiu          \n");
    printf("|  Version: 0.0             \n");
    printf("|  MCU: GD32                \n");
    printf("==================================\n");
    a_task_h = Task_Create(a_task, NULL);
    b_task_h = Task_Create(b_task, NULL);
    c_task_h = Task_Create(c_task, NULL);
    printf("System Starting...\n");
    Task_StartScheduler();
    while (1) {
    };
}
