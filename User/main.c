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


#define PRODUCER_PRIO       2  // 生产者任务优先级 (与协作任务相同)
#define CONSUMER_PRIO       1  // 消费者任务优先级 (与后台任务相同，较低)
#define INSPECTOR_PRIO      4  // 质检员任务优先级 (与中断任务相同，很高)

typedef struct {
    uint32_t id;
    uint32_t data;
} Product_t;

QueueHandle_t product_queue; // 消息队列句柄

TaskHandle_t producer_task_h = NULL;
TaskHandle_t consumer_task_h = NULL;
TaskHandle_t inspector_task_h = NULL;

MutexHandle_t print_lock;
TaskHandle_t a_task_h = NULL;
TaskHandle_t b_task_h = NULL;
TaskHandle_t c_task_h = NULL; // 由 d_task 创建
TaskHandle_t d_task_h = NULL;
TaskHandle_t e_task_h = NULL;
TaskHandle_t high_prio_task_h = NULL;
TaskHandle_t interrupt_task_h = NULL;
TaskHandle_t background_task_h = NULL;
TimerHandle_t perio_timer_h = NULL;
TimerHandle_t single_timer_h = NULL;

MutexHandle_t recursive_lock; // 测试递归锁
TaskHandle_t recursive_task_h = NULL; // 测试递归锁的任务

static int scheduler_started = 0;

#define SAFE_PRINTF(...)                                \
do {                                                    \
    if (scheduler_started) {                            \
        Mutex_Lock(print_lock);                         \
        printf(__VA_ARGS__);                            \
        Mutex_Unlock(print_lock);                       \
    } else {                                            \
        printf(__VA_ARGS__);                            \
    }                                                   \
} while (0)


void perio_timer_cb(TimerHandle_t timer) {
    SAFE_PRINTF("周期性 定时器 执行: %llu\n", MyRTOS_GetTick());
}

void single_timer_cb(TimerHandle_t timer) {
    SAFE_PRINTF("单次 定时器 执行: %llu\n", MyRTOS_GetTick());
}

void a_task(void *param) {
    static uint16_t i = 0;

    SAFE_PRINTF("任务 A (Prio %d) 启动\n", COLLABORATION_TASKS_PRIO);

    while (1) {
        Task_Wait();
        while (1) {
            i++;

            SAFE_PRINTF("任务 A: 尝试获取递归锁...\n");


            Mutex_Lock_Recursive(recursive_lock); // 使用普通 Lock 也行，这里为了统一


            SAFE_PRINTF("任务 A: 成功获取递归锁 运行一次, 然后释放。\n");


            Mutex_Unlock_Recursive(recursive_lock);


            SAFE_PRINTF("任务 A 正在运行,第 %d 次\n", i);


            if (i == 5) {
                i = 0;

                SAFE_PRINTF("任务 A 唤醒 任务 B, 并开始等待 任务 B 的唤醒\n");

                Task_Notify(b_task_h);
                break;
            }
            Task_Delay(1000); //1s
        }
    }
}

void b_task(void *param) {
    static uint16_t i = 0;

    SAFE_PRINTF("任务 B (Prio %d) 启动\n", COLLABORATION_TASKS_PRIO);

    while (1) {
        Task_Wait();
        while (1) {
            SAFE_PRINTF("任务 B 正在运行,第 %d 次\n", i);

            i++;
            if (i == 3) {
                i = 0;

                SAFE_PRINTF("任务 B 唤醒 任务 A, 并开始等待 任务 A 的唤醒\n");

                Task_Notify(a_task_h);
                break;
            }
            Task_Delay(1000); //1s
        }
    }
}

void c_task(void *param) {
    uint16_t index = 0;

    SAFE_PRINTF("任务 C (Prio %d) 启动\n", COLLABORATION_TASKS_PRIO);

    while (1) {
        index++;

        SAFE_PRINTF("任务 C 正在运行,第 %d 次\n", index);

        if (index == 5) {
            SAFE_PRINTF("任务 C 删除 任务 C\n");

            // 任务自删除后，这个句柄会失效
            Task_Delete(c_task_h);
        }
        Task_Delay(1000);
    }
}

void d_task(void *param) {
    uint16_t index = 0;

    SAFE_PRINTF("任务 D (Prio %d) 启动\n", COLLABORATION_TASKS_PRIO);

    while (1) {
        // 检查 c_task_h 是否为 NULL 或任务已删除
        if (c_task_h == NULL || Task_GetState(c_task_h) == TASK_STATE_UNUSED) {
            index++;
            if (index >= 5) {
                SAFE_PRINTF("任务 D 将 创建 任务 C\n");

                c_task_h = Task_Create(c_task, 256,NULL, COLLABORATION_TASKS_PRIO);
                index = 0;
            }
        }

        SAFE_PRINTF("任务 D 正在运行, 检查次数 %d\n", index);

        Task_Delay(1000);
    }
}

void e_task(void *param) {
    SAFE_PRINTF("任务 E (Prio %d) 启动, 闪烁 PB2\n", COLLABORATION_TASKS_PRIO);

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
    SAFE_PRINTF("后台任务 (Prio %d) 启动, 闪烁 PB0\n", BACKGROUND_TASK_PRIO);

    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_0);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_0);
    while (1) {
        gpio_bit_toggle(GPIOB, GPIO_PIN_0);
        Task_Delay(1000);
    }
}

void high_prio_task(void *param) {
    SAFE_PRINTF("高优任务 (Prio %d) 启动\n", HIGH_PRIO_TASK_PRIO);

    while (1) {
        Task_Delay(5000); // 每 5 秒抢占一次

        SAFE_PRINTF("\n<<<<<<<<<< [高优先级任务抢占] >>>>>>>>>>\n\n");
    }
}

void interrupt_handler_task(void *param) {
    SAFE_PRINTF("任务中断 (Prio %d) 启动, 等待按键...\n", INTERRUPT_TASK_PRIO);

    while (1) {
        Task_Wait();

        SAFE_PRINTF("\n\n!!!!!!!!!! [中断紧急事件处理] !!!!!!!!!!\n\n");
    }
}


void producer_task(void *param) {
    Product_t product;
    product.id = 0;
    product.data = 100;

    SAFE_PRINTF("生产者 启动 (Prio %d)", PRODUCER_PRIO);

    while (1) {
        product.id++;
        product.data += 10;

        SAFE_PRINTF("生产者: 发送产品 ID %d\n", product.id);

        // 发送产品到队列，如果队列满了会阻塞等待
        if (Queue_Send(product_queue, &product, MY_RTOS_MAX_DELAY)) {
            SAFE_PRINTF("生产者: 成功发送产品 ID %d\n", product.id);
        } else {
            SAFE_PRINTF("生产者: 队列已满, 放弃发送产品 ID %d\n", product.id);
        }
        Task_Delay(2000); // 每 2 秒生产一个
    }
}

/**
 * @brief 消费者任务 (低优先级)
 */
void consumer_task(void *param) {
    Product_t received_product;

    SAFE_PRINTF("消费者 启动 (Prio %d)", CONSUMER_PRIO);

    while (1) {
        SAFE_PRINTF("消费者: 等待接收产品...\n");

        // 从队列接收产品，如果队列为空会阻塞等待
        if (Queue_Receive(product_queue, &received_product, MY_RTOS_MAX_DELAY)) {
            SAFE_PRINTF("消费者: 接收到产品 ID %d\n", received_product.id);
        } else {
            SAFE_PRINTF("消费者: 队列已空, 放弃接收产品\n");
        }
        // 没有延时，处理完一个马上就去等下一个
    }
}

/**
 * @brief 质检员任务 (高优先级)
 */
void inspector_task(void *param) {
    Product_t received_product;


    SAFE_PRINTF("质检员 启动 (Prio %d)", INSPECTOR_PRIO);


    while (1) {
        SAFE_PRINTF("质检员: 等待接收产品...\n");

        // 同样从队列接收，但因为优先级高，它会优先被唤醒
        if (Queue_Receive(product_queue, &received_product, MY_RTOS_MAX_DELAY)) {
            SAFE_PRINTF("质检员: 拦截 到产品 ID %d\n", received_product.id);
        } else {
            SAFE_PRINTF("质检员: 队列已空, 放弃 拦截 产品\n");
        }
        // 质检员检查完后，休息 5 秒，给消费者一些机会
        Task_Delay(5000);
    }
}


void sub_function_needs_lock(int level) {
    SAFE_PRINTF("递归任务 子函数 %d 加锁\n", level);

    Mutex_Lock_Recursive(recursive_lock);

    SAFE_PRINTF("递归任务 子函数 %d 已加锁\n", level);

    Task_Delay(500);

    SAFE_PRINTF("递归任务 子函数 %d 解锁\n", level);
    Mutex_Unlock_Recursive(recursive_lock);
}

void recursive_test_task(void *param) {
    SAFE_PRINTF("递归任务 启动 (Prio %d)\n", COLLABORATION_TASKS_PRIO);

    while (1) {
        SAFE_PRINTF("递归任务 主循环 开始测试 (3层)\n");

        Mutex_Lock_Recursive(recursive_lock);
        SAFE_PRINTF("递归任务 主循环 加锁 (1层)\n");

        sub_function_needs_lock(2);
        sub_function_needs_lock(3);

        SAFE_PRINTF("递归任务 主循环 解锁 (1层)\n");
        Mutex_Unlock_Recursive(recursive_lock);

        SAFE_PRINTF("递归任务 已完全解锁 延时 3秒 给其他任务机会\n");

        Task_Delay(3000);

        SAFE_PRINTF("递归任务 延时结束\n");

        Task_Delay(2000);
    }
}



void boot_task(void *param) {
    Task_Create(recursive_test_task, 512, NULL, COLLABORATION_TASKS_PRIO);
    a_task_h = Task_Create(a_task, 256, NULL, COLLABORATION_TASKS_PRIO);
    b_task_h = Task_Create(b_task, 256,NULL, COLLABORATION_TASKS_PRIO);
    d_task_h = Task_Create(d_task, 256,NULL, COLLABORATION_TASKS_PRIO);
    e_task_h = Task_Create(e_task, 256, NULL, COLLABORATION_TASKS_PRIO);
    background_task_h = Task_Create(background_blinky_task, 256, NULL, BACKGROUND_TASK_PRIO);
    high_prio_task_h = Task_Create(high_prio_task, 256,NULL, HIGH_PRIO_TASK_PRIO);
    interrupt_task_h = Task_Create(interrupt_handler_task, 256,NULL, INTERRUPT_TASK_PRIO);
    Task_Delay(200);
    Task_Notify(a_task_h);


    SAFE_PRINTF("队列 启动\n");

    // 创建一个能容纳 3 个 Product_t 的队列
    product_queue = Queue_Create(3, sizeof(Product_t));
    if (product_queue == NULL) {
        SAFE_PRINTF("队列创建失败\n");
    }
    // 创建队列相关的任务
    consumer_task_h = Task_Create(consumer_task, 256,NULL, CONSUMER_PRIO);
    producer_task_h = Task_Create(producer_task, 256,NULL, PRODUCER_PRIO);
    inspector_task_h = Task_Create(inspector_task, 256, NULL, INSPECTOR_PRIO);
    //创建定时器
    perio_timer_h = Timer_Create(10000, 10000, perio_timer_cb, NULL);
    single_timer_h = Timer_Create(5000, 0, single_timer_cb, NULL);


    SAFE_PRINTF("定时器 启动\n");


    Timer_Start(perio_timer_h);
    Timer_Start(single_timer_h);


    SAFE_PRINTF("=============================================\n\n");

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
            Task_Notify(interrupt_task_h);
        }
        exti_interrupt_flag_clear(EXTI_0);
    }
}

void sys_config() {
    lib_usart0_init();
    print_lock = Mutex_Create();
    recursive_lock = Mutex_Create();
    if (recursive_lock == NULL) {
        SAFE_PRINTF("递归锁创建失败!\n");
    }
}


int main(void) {
    sys_config();
    key_exti_init();
    SAFE_PRINTF("=========   RTOS Demo   =========\n");
    SAFE_PRINTF("|  Author: XiaoXiu                   \n");
    SAFE_PRINTF("|  Version: 1.0 (Priority-based)     \n");
    SAFE_PRINTF("|  MCU: GD32                         \n");
    SAFE_PRINTF("==========================================\n");
    Task_Create(boot_task, 256, NULL, 0);
    SAFE_PRINTF("System Starting...\n");
    Task_StartScheduler();
    while (1) {
    };
}
