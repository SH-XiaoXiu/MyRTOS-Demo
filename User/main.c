#include "gd32f4xx.h"
#include "MyRTOS.h"
#include "MyRTOS_Log.h"
#include "MyRTOS_Monitor.h"
#include <string.h>

#include "gd32f4xx_exti.h"
#include "gd32f4xx_gpio.h"
#include "gd32f4xx_rcu.h"
#include "gd32f4xx_syscfg.h"
#include "lib_usart0.h"

// 任务优先级定义
#define BACKGROUND_TASK_PRIO       1
#define CONSUMER_PRIO              1
#define COLLABORATION_TASKS_PRIO   2
#define PRODUCER_PRIO              2
#define HIGH_PRIO_TASK_PRIO        3
#define INTERRUPT_TASK_PRIO        4
#define INSPECTOR_PRIO             4

typedef struct {
    uint32_t id;
    uint32_t data;
} Product_t;

// 全局句柄
QueueHandle_t product_queue;
MutexHandle_t recursive_lock;

TaskHandle_t producer_task_h = NULL;
TaskHandle_t consumer_task_h = NULL;
TaskHandle_t inspector_task_h = NULL;
TaskHandle_t a_task_h = NULL;
TaskHandle_t b_task_h = NULL;
TaskHandle_t c_task_h = NULL;
TaskHandle_t d_task_h = NULL;
TaskHandle_t e_task_h = NULL;
TaskHandle_t high_prio_task_h = NULL;
TaskHandle_t interrupt_task_h = NULL;
TaskHandle_t background_task_h = NULL;
TimerHandle_t perio_timer_h = NULL;
TimerHandle_t single_timer_h = NULL;
TaskHandle_t recursive_task_h = NULL;

void perio_timer_cb(TimerHandle_t timer) {
    SYS_LOGI("周期性 定时器 执行\n");
}

void single_timer_cb(TimerHandle_t timer) {
    SYS_LOGI("单次 定时器 执行\n");
}

void a_task(void *param) {
    static uint16_t i = 0;
    while (1) {
        Task_Wait();
        while (1) {
            i++;
            PRINT("任务 A: 尝试获取递归锁...\n");
            Mutex_Lock_Recursive(recursive_lock);
            PRINT("任务 A: 成功获取递归锁 运行一次, 然后释放。\n");
            Mutex_Unlock_Recursive(recursive_lock);
            PRINT("任务 A 正在运行,第 %d 次\n", i);

            if (i == 5) {
                i = 0;
                SYS_LOGI("任务 A 唤醒 任务 B, 并等待\n");
                Task_Notify(b_task_h);
                break;
            }
            Task_Delay(MS_TO_TICKS(1000));
        }
    }
}

void b_task(void *param) {
    static uint16_t i = 0;
    while (1) {
        Task_Wait();
        while (1) {
            PRINT("任务 B 正在运行,第 %d 次\n", i++);
            if (i == 3) {
                i = 0;
                SYS_LOGI("任务 B 唤醒 任务 A, 并等待\n");
                Task_Notify(a_task_h);
                break;
            }
            Task_Delay(MS_TO_TICKS(1000));
        }
    }
}

void c_task(void *param) {
    uint16_t index = 0;
    while (1) {
        index++;
        PRINT("任务 C 正在运行,第 %d 次\n", index);
        if (index == 5) {
            SYS_LOGW("任务 C 删除自己\n");
            Task_Delete(NULL);
        }
        Task_Delay(MS_TO_TICKS(1000));
    }
}

void d_task(void *param) {
    uint16_t index = 0;
    while (1) {
        if (c_task_h == NULL || Task_GetState(c_task_h) == TASK_STATE_UNUSED) {
            index++;
            if (index >= 5) {
                SYS_LOGI("任务 D 创建 任务 C\n");
                c_task_h = Task_Create(c_task, "TaskC_dynamic", 256, NULL, COLLABORATION_TASKS_PRIO);
                index = 0;
            }
        }
        PRINT("任务 D 正在运行, 检查次数 %d\n", index);
        Task_Delay(MS_TO_TICKS(1000));
    }
}

void e_task(void *param) {
    rcu_periph_clock_enable(RCU_GPIOB);
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_2);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_2);
    GPIO_BC(GPIOB) = GPIO_PIN_2;
    while (1) {
        gpio_bit_set(GPIOB, GPIO_PIN_2);
        Task_Delay(MS_TO_TICKS(250));
        gpio_bit_reset(GPIOB, GPIO_PIN_2);
        Task_Delay(MS_TO_TICKS(250));
    }
}

void background_blinky_task(void *param) {
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_0);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_0);
    while (1) {
        gpio_bit_toggle(GPIOB, GPIO_PIN_0);
        Task_Delay(MS_TO_TICKS(1000));
    }
}

void high_prio_task(void *param) {
    while (1) {
        Task_Delay(MS_TO_TICKS(5000));
        PRINT("\n<<<<<<<<<< [高优先级任务抢占] >>>>>>>>>>>\n\n");
    }
}

void interrupt_handler_task(void *param) {
    while (1) {
        Task_Wait();
        if (MyRTOS_Monitor_IsRunning()) {
            MyRTOS_Monitor_Stop();
        } else {
            MyRTOS_Monitor_Start();
        }
    }
}

void producer_task(void *param) {
    Product_t product = {0, 100};
    while (1) {
        product.id++;
        product.data += 10;
        PRINT("生产者: 生产产品 ID %lu\n", product.id);
        if (!Queue_Send(product_queue, &product, MY_RTOS_MAX_DELAY)) {
            SYS_LOGW("生产者: 发送产品 ID %lu 失败\n", product.id);
        }
        Task_Delay(MS_TO_TICKS(2000));
    }
}

void consumer_task(void *param) {
    Product_t received_product;
    while (1) {
        PRINT("消费者: 等待产品...\n");
        if (Queue_Receive(product_queue, &received_product, MY_RTOS_MAX_DELAY)) {
            PRINT("消费者: 接收到产品 ID %lu\n", received_product.id);
        }
    }
}

void inspector_task(void *param) {
    Product_t received_product;
    while (1) {
        PRINT("质检员: 等待拦截产品...\n");
        if (Queue_Receive(product_queue, &received_product, MY_RTOS_MAX_DELAY)) {
            SYS_LOGW("质检员: 拦截到产品 ID %lu\n", received_product.id);
        }
        Task_Delay(MS_TO_TICKS(5000));
    }
}

void sub_function_needs_lock(int level) {
    PRINT("递归任务: 子函数 %d 尝试加锁\n", level);
    Mutex_Lock_Recursive(recursive_lock);
    PRINT("递归任务: 子函数 %d 已加锁\n", level);
    Task_Delay(MS_TO_TICKS(500));
    Mutex_Unlock_Recursive(recursive_lock);
    PRINT("递归任务: 子函数 %d 已解锁\n", level);
}

void recursive_test_task(void *param) {
    while (1) {
        SYS_LOGI("递归任务: 开始测试\n");
        Mutex_Lock_Recursive(recursive_lock);
        PRINT("递归任务: 主循环加锁 (1层)\n");
        sub_function_needs_lock(2);
        sub_function_needs_lock(3);
        Mutex_Unlock_Recursive(recursive_lock);
        PRINT("递归任务: 主循环解锁 (1层)\n");
        SYS_LOGI("递归任务: 已完全解锁，等待3秒\n");
        Task_Delay(MS_TO_TICKS(3000));
    }
}

void boot_task(void *param) {
    SYS_LOGI("Boot task starting...\n");
    recursive_lock = Mutex_Create();

    // 创建任务并命名
    recursive_task_h = Task_Create(recursive_test_task, "RecursiveTask", 512, NULL, COLLABORATION_TASKS_PRIO);
    a_task_h = Task_Create(a_task, "TaskA", 256, NULL, COLLABORATION_TASKS_PRIO);
    b_task_h = Task_Create(b_task, "TaskB", 256, NULL, COLLABORATION_TASKS_PRIO);
    d_task_h = Task_Create(d_task, "TaskD_Creator", 256, NULL, COLLABORATION_TASKS_PRIO);
    e_task_h = Task_Create(e_task, "LED_Blinky_PB2", 256, NULL, COLLABORATION_TASKS_PRIO);
    background_task_h = Task_Create(background_blinky_task, "BG_Blinky_PB0", 256, NULL, BACKGROUND_TASK_PRIO);
    high_prio_task_h = Task_Create(high_prio_task, "HighPrioTask", 256, NULL, HIGH_PRIO_TASK_PRIO);
    interrupt_task_h = Task_Create(interrupt_handler_task, "KeyHandlerTask", 256, NULL, INTERRUPT_TASK_PRIO);

    Task_Delay(MS_TO_TICKS(200));
    Task_Notify(a_task_h);

    product_queue = Queue_Create(3, sizeof(Product_t));
    if (product_queue) {
        consumer_task_h = Task_Create(consumer_task, "Consumer", 256, NULL, CONSUMER_PRIO);
        producer_task_h = Task_Create(producer_task, "Producer", 256, NULL, PRODUCER_PRIO);
        inspector_task_h = Task_Create(inspector_task, "Inspector", 256, NULL, INSPECTOR_PRIO);
    } else {
        SYS_LOGE("Product queue creation failed!\n");
    }

    perio_timer_h = Timer_Create(MS_TO_TICKS(10000), MS_TO_TICKS(10000), perio_timer_cb, NULL);
    single_timer_h = Timer_Create(MS_TO_TICKS(5000), 0, single_timer_cb, NULL);
    Timer_Start(perio_timer_h);
    Timer_Start(single_timer_h);

    SYS_LOGI("All initial tasks and services created.\n");
    SYS_LOGI("Boot task deleting itself.\n");
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
    key_exti_init();
}

int main(void) {
    sys_config();
    SYS_LOGI("=========   MyRTOS v2.1 (Named Tasks)   =========\n");
    SYS_LOGI("|  Author: XiaoXiu (Refactored by AI)\n");
    SYS_LOGI("|  Press USER key to toggle Monitor\n");
    SYS_LOGI("================================================\n");
    Task_Create(boot_task, "BootTask", 512, NULL, 2);
    SYS_LOGI("System Starting Scheduler...\n");
    Task_StartScheduler();

    while (1) {};
}
