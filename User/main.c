#include <stdint.h>
#include <stdio.h>

#include "gd32f4xx_gpio.h"
#include "gd32f4xx_rcu.h"
#include "lib_usart0.h"
#include "MyRTOS.h"

#define A_TASK 0
#define B_TASK 1
#define C_TASK 2

Mutex_t print_lock;
Mutex_t del_lock;
Task_t *boot_h = NULL;
Task_t *a_task_h = NULL;
Task_t *b_task_h = NULL;
Task_t *c_task_h = NULL;
Task_t *d_task_h = NULL;
Task_t *e_task_h = NULL;


void a_task(void *param) {
    static uint8_t i = 0;
    Mutex_Lock(&print_lock);
    printf("任务 A 启动\n");
    Mutex_Unlock(&print_lock);
    while (1) {
        Task_Wait();
        while (1) {
            i++;
            Mutex_Lock(&print_lock);
            printf("任务 A 正在运行,第 %d 次\n", i);
            Mutex_Unlock(&print_lock);
            if (i == 5) {
                i = 0;
                Mutex_Lock(&print_lock);
                printf("任务 A 唤醒 任务 B, 并开始等待 任务 B 的唤醒\n");
                Mutex_Unlock(&print_lock);
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
            Mutex_Lock(&print_lock);\
            printf("任务 B 正在运行,第 %d 次\n", i);
            Mutex_Unlock(&print_lock);
            i++;
            if (i == 3) {
                i = 0;
                Mutex_Lock(&print_lock);
                printf("任务 B 唤醒 任务 A, 并开始等待 任务 A 的唤醒\n");
                Mutex_Unlock(&print_lock);
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
    Mutex_Unlock(&print_lock);
    while (1) {
        index++;
        Mutex_Lock(&print_lock);
        printf("任务 C 正在运行,第 %d 次\n", index);
        Mutex_Unlock(&print_lock);
        if (index == 5) {
            index = 0;
            printf("任务 C 删除 任务 C\n");
            Task_Delete(c_task_h);
        }
        Task_Delay(1000);
    }
}

void d_task(void *param) {
    uint32_t index = 0;
    Mutex_Lock(&print_lock);
    printf("任务 D 启动\n");
    Mutex_Unlock(&print_lock);
    while (1) {
        if (c_task_h == NULL || c_task_h->state == TASK_STATE_UNUSED) {
            index++;
            if (index == 5) {
                printf("任务 D 将 创建 任务 C\n");
                c_task_h = Task_Create(c_task, NULL);
                index = 0;
            }
        }
        Mutex_Lock(&print_lock);
        printf("任务 D 正在运行,第 %d 次\n", index);
        Mutex_Unlock(&print_lock);
        Task_Delay(1000);
    }
}

void e_task(void *param) {
    printf("任务 E 启动\n");
    rcu_periph_clock_enable(RCU_GPIOB);
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,GPIO_PIN_2);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,GPIO_PIN_2);
    GPIO_BC(GPIOB) = GPIO_PIN_2;
    while (1) {
        gpio_bit_set(GPIOB, GPIO_PIN_2);
        Task_Delay(500);
        gpio_bit_reset(GPIOB, GPIO_PIN_2);
        Task_Delay(500);
    }
}

void boot(void *param) {
    a_task_h = Task_Create(a_task, NULL);
    b_task_h = Task_Create(b_task, NULL);
    d_task_h = Task_Create(d_task, NULL);
    e_task_h = Task_Create(e_task, NULL);
    Task_Delay(100);
    Task_Notify(a_task_h->taskId);
    Task_Delete(boot_h);
}


void sys_config() {
    lib_usart0_init();
    Mutex_Init(&print_lock);
    Mutex_Init(&del_lock);
}


/**
 * @brief 一个ARM32的RTOS简易实现
 * @author A_XiaoXiu
 * @remark 参考如下
 * \n
 * FreeRTOS: https://github.com/FreeRTOS/FreeRTOS-Kernel
 * \n
 * AI: https://chatgpt.com/ -> PSP汇编部分
 * 最简易实现, 无任务优先级
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
    boot_h = Task_Create(boot, NULL);
    printf("System Starting...\n");
    Task_StartScheduler();
    while (1) {
    };
}
