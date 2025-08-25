#include <stdint.h>
#include <stdio.h>

#include "gd32f4xx_gpio.h"
#include "gd32f4xx_rcu.h"
#include "gd32f4xx_exti.h"
#include "gd32f4xx_syscfg.h"
#include "lib_usart0.h"
#include "MyRTOS.h"

#define BACKGROUND_TASK_PRIO       1  // 后台任务
#define COLLABORATION_TASKS_PRIO   2  // 协作任务组
#define HIGH_PRIO_TASK_PRIO        3  // 高优先级周期任务
#define INTERRUPT_TASK_PRIO        4  // 中断响应任务 优先级最高

Mutex_t print_lock;
Task_t *a_task_h = NULL;
Task_t *b_task_h = NULL;
Task_t *c_task_h = NULL; // 由 d_task 创建
Task_t *d_task_h = NULL;
Task_t *e_task_h = NULL;
Task_t *high_prio_task_h = NULL;
Task_t *interrupt_task_h = NULL;
Task_t *background_task_h = NULL;


void a_task(void *param) {
    static uint8_t i = 0;
    Mutex_Lock(&print_lock);
    printf("任务 A (Prio %d) 启动\n", COLLABORATION_TASKS_PRIO);
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
    printf("任务 B (Prio %d) 启动\n", COLLABORATION_TASKS_PRIO);
    Mutex_Unlock(&print_lock);
    while (1) {
        Task_Wait();
        while (1) {
            Mutex_Lock(&print_lock);
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
    printf("任务 C (Prio %d) 启动\n", COLLABORATION_TASKS_PRIO);
    Mutex_Unlock(&print_lock);
    while (1) {
        index++;
        Mutex_Lock(&print_lock);
        printf("任务 C 正在运行,第 %d 次\n", index);
        Mutex_Unlock(&print_lock);
        if (index == 5) {
            Mutex_Lock(&print_lock);
            printf("任务 C 删除 任务 C\n");
            Mutex_Unlock(&print_lock);
            // 任务自删除后，这个句柄会失效
            Task_Delete(c_task_h);
        }
        Task_Delay(1000);
    }
}

void d_task(void *param) {
    uint32_t index = 0;
    Mutex_Lock(&print_lock);
    printf("任务 D (Prio %d) 启动\n", COLLABORATION_TASKS_PRIO);
    Mutex_Unlock(&print_lock);
    while (1) {
        // 检查 c_task_h 是否为 NULL 或任务已删除
        if (c_task_h == NULL || c_task_h->state == TASK_STATE_UNUSED) {
            index++;
            if (index >= 5) {
                Mutex_Lock(&print_lock);
                printf("任务 D 将 创建 任务 C\n");
                Mutex_Unlock(&print_lock);
                c_task_h = Task_Create(c_task, NULL, COLLABORATION_TASKS_PRIO);
                index = 0;
            }
        }
        Mutex_Lock(&print_lock);
        printf("任务 D 正在运行, 检查次数 %d\n", index);
        Mutex_Unlock(&print_lock);
        Task_Delay(1000);
    }
}

void e_task(void *param) {
    Mutex_Lock(&print_lock);
    printf("任务 E (Prio %d) 启动, 闪烁 PB2\n", COLLABORATION_TASKS_PRIO);
    Mutex_Unlock(&print_lock);
    rcu_periph_clock_enable(RCU_GPIOB);
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_2);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_2);
    GPIO_BC(GPIOB) = GPIO_PIN_2;
    while (1) {
        gpio_bit_set(GPIOB, GPIO_PIN_2);
        Task_Delay(250);
        gpio_bit_reset(GPIOB, GPIO_PIN_2);
        Task_Delay(250);
    }
}


// 后台任务, 闪烁另一个LED (PB0)
void background_blinky_task(void *param) {
    Mutex_Lock(&print_lock);
    printf("后台任务 (Prio %d) 启动, 闪烁 PB0\n", BACKGROUND_TASK_PRIO);
    Mutex_Unlock(&print_lock);
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_0);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_0);
    while (1) {
        gpio_bit_toggle(GPIOB, GPIO_PIN_0);
        Task_Delay(1000);
    }
}

void high_prio_task(void *param) {
    Mutex_Lock(&print_lock);
    printf("高优任务 (Prio %d) 启动\n", HIGH_PRIO_TASK_PRIO);
    Mutex_Unlock(&print_lock);
    while (1) {
        Task_Delay(5000); // 每 5 秒抢占一次
        Mutex_Lock(&print_lock);
        printf("\n<<<<<<<<<< [高优先级任务抢占] >>>>>>>>>>\n\n");
        Mutex_Unlock(&print_lock);
    }
}

void interrupt_handler_task(void *param) {
    Mutex_Lock(&print_lock);
    printf("中断任务 (Prio %d) 启动, 等待按键...\n", INTERRUPT_TASK_PRIO);
    Mutex_Unlock(&print_lock);
    while (1) {
        Task_Wait();
        Mutex_Lock(&print_lock);
        printf("\n\n!!!!!!!!!! [中断紧急事件处理] !!!!!!!!!!\n\n");
        Mutex_Unlock(&print_lock);
    }
}

void boot_task(void *param) {
    a_task_h = Task_Create(a_task, NULL, COLLABORATION_TASKS_PRIO);
    b_task_h = Task_Create(b_task, NULL, COLLABORATION_TASKS_PRIO);
    d_task_h = Task_Create(d_task, NULL, COLLABORATION_TASKS_PRIO);
    e_task_h = Task_Create(e_task, NULL, COLLABORATION_TASKS_PRIO);
    background_task_h = Task_Create(background_blinky_task, NULL, BACKGROUND_TASK_PRIO);
    high_prio_task_h = Task_Create(high_prio_task, NULL, HIGH_PRIO_TASK_PRIO);
    interrupt_task_h = Task_Create(interrupt_handler_task, NULL, INTERRUPT_TASK_PRIO);
    Task_Delay(200);
    Task_Notify(a_task_h->taskId);
    Task_Delete(NULL);
}


void key_exti_init(void) {
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_SYSCFG);
    gpio_mode_set(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_0);
    syscfg_exti_line_config(EXTI_SOURCE_GPIOA, EXTI_SOURCE_PIN0);
    exti_init(EXTI_0, EXTI_INTERRUPT, EXTI_TRIG_FALLING);
    exti_interrupt_flag_clear(EXTI_0);
    NVIC_SetPriority(EXTI0_IRQn, 5);
    NVIC_EnableIRQ(EXTI0_IRQn);
}

void EXTI0_IRQHandler(void) {
    if (exti_interrupt_flag_get(EXTI_0) != RESET) {
        if (interrupt_task_h != NULL) {
            Task_Notify(interrupt_task_h->taskId);
        }
        exti_interrupt_flag_clear(EXTI_0);
    }
}

void sys_config() {
    lib_usart0_init();
    Mutex_Init(&print_lock);
}


int main(void) {
    sys_config();
    key_exti_init();
    printf("=========   RTOS Priority Demo   =========\n");
    printf("|  Author: XiaoXiu                   \n");
    printf("|  Version: 1.0 (Priority-based)     \n");
    printf("|  MCU: GD32                         \n");
    printf("==========================================\n");
    Task_Create(boot_task, NULL, 0);
    printf("System Starting...\n");
    Task_StartScheduler();
    while (1) {
    };
}
