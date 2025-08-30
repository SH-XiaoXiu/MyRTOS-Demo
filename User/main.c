#include "MyRTOS.h"
#include "MyRTOS_Shell.h"
#include "MyRTOS_Driver_Timer.h"
#include "gd32f4xx_exti.h"
#include "gd32f4xx_gpio.h"
#include "gd32f4xx_rcu.h"
#include "gd32f4xx_syscfg.h"
#include <string.h>

#include "MyRTOS_Std.h"


// 任务优先级定义
#define BACKGROUND_TASK_PRIO       1
#define CONSUMER_PRIO              1
#define COLLABORATION_TASKS_PRIO   2
#define PRODUCER_PRIO              2
#define TIMER_TEST_TASK_PRIO       2
#define LOGGER_TASK_PRIO           2
#define ISR_TEST_TASK_PRIO         3
#define HIGH_PRIO_TASK_PRIO        3
#define KEY_HANDLER_TASK_PRIO      4
#define INSPECTOR_PRIO             4
#define BOOT_TASK_PRIO             2


// 全局句柄
QueueHandle_t product_queue;
MutexHandle_t recursive_lock;
SemaphoreHandle_t logger_semaphore;
SemaphoreHandle_t isr_semaphore; // 用于中断测试的信号量
TaskHandle_t producer_task_h, consumer_task_h, inspector_task_h;
TaskHandle_t a_task_h, b_task_h, c_task_h, d_task_h, e_task_h;
TaskHandle_t high_prio_task_h, key_handler_task_h, background_task_h;
#if (MY_RTOS_USE_SOFTWARE_TIMERS == 1)
TimerHandle_t perio_timer_h, single_timer_h;
#endif
TaskHandle_t recursive_task_h, timer_test_task_h;
TaskHandle_t logger_task1_h, logger_task2_h, logger_task3_h;
TaskHandle_t isr_test_task_h;

typedef struct {
    uint32_t id;
    uint32_t data;
} Product_t;

// --- 任务和回调函数声明 ---
#if (MY_RTOS_USE_SOFTWARE_TIMERS == 1)
void perio_timer_cb(TimerHandle_t timer);

void single_timer_cb(TimerHandle_t timer);
#endif

void a_task(void *param);

void b_task(void *param);

void c_task(void *param);

void d_task(void *param);

void e_task(void *param);

void background_blinky_task(void *param);

void high_prio_task(void *param);

void key_handler_task(void *param);

void isr_test_task(void *param);

void producer_task(void *param);

void consumer_task(void *param);

void inspector_task(void *param);

void recursive_test_task(void *param);

void timer_test_task(void *param);

void logger_task(void *param);

void boot_task(void *param);

void key_exti_init(void);

// 外部函数
extern void MyRTOS_Platform_TerminalCommandsInit(void);


// --- 任务和回调函数实现 ---

#if (MY_RTOS_USE_SOFTWARE_TIMERS == 1)
void perio_timer_cb(TimerHandle_t timer) {
    SYS_LOGI("周期性 定时器 执行");
}

void single_timer_cb(TimerHandle_t timer) {
    SYS_LOGI("单次 定时器 执行");
}
#endif

void a_task(void *param) {
    static uint16_t i = 0;
    while (1) {
        Task_Wait();
        USER_LOGD("任务 A: 开始运行");
        for (i = 1; i <= 5; ++i) {
            USER_LOGD("任务 A: 计数 %d", i);
            Task_Delay(MS_TO_TICKS(1000));
        }
        i = 0;
        SYS_LOGI("任务 A 唤醒 任务 B, 并等待");
        Task_Notify(b_task_h);
    }
}

void b_task(void *param) {
    static uint16_t i = 0;
    while (1) {
        Task_Wait();
        USER_LOGD("任务 B: 开始运行");
        for (i = 1; i <= 3; ++i) {
            USER_LOGD("任务 B: 计数 %d", i);
            Task_Delay(MS_TO_TICKS(1000));
        }
        i = 0;
        SYS_LOGI("任务 B 唤醒 任务 A, 并等待");
        Task_Notify(a_task_h);
    }
}

void c_task(void *param) {
    uint16_t index = 0;
    while (1) {
        index++;
        USER_LOGD("任务 C 正在运行,第 %d 次", index);
        if (index == 5) {
            SYS_LOGW("任务 C 删除自己");
            Task_Delete(NULL);
        }
        Task_Delay(MS_TO_TICKS(1000));
    }
}

void d_task(void *param) {
    uint16_t check_count = 0;
    while (1) {
        if (c_task_h == NULL || Task_GetState(c_task_h) == TASK_STATE_UNUSED) {
            check_count++;
            if (check_count >= 5) {
                SYS_LOGI("任务 D 检测到任务C不存在, 重新创建");
                c_task_h = Task_Create(c_task, "TaskC_dynamic", 256, NULL, COLLABORATION_TASKS_PRIO);
                if (c_task_h == NULL) {
                    SYS_LOGE("任务 D 创建任务 C 失败");
                }
                check_count = 0;
            }
        }
        USER_LOGD("任务 D 正在运行, 检查次数 %d", check_count);
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
        USER_LOGD("\n<<<<<<<<<< [高优先级任务抢占] >>>>>>>>>>>\n");
    }
}

void key_handler_task(void *param) {
    while (1) {
        Task_Wait(); // 等待来自ISR的通知
        SYS_LOGW("按键处理任务: 已被中断唤醒, 运行Monitor");
#if (MY_RTOS_USE_MONITOR == 1)
        if (MyRTOS_Monitor_IsRunning()) {
            MyRTOS_Monitor_Stop();
        } else {
            MyRTOS_Monitor_Start();
        }
#endif
    }
}

void isr_test_task(void *param) {
    SYS_LOGI("ISR测试任务: 启动并等待信号量...");
    while (1) {
        if (Semaphore_Take(isr_semaphore, MY_RTOS_MAX_DELAY)) {
            SYS_LOGI("ISR测试任务: [?] 成功从ISR获取信号量!");
        }
    }
}

void producer_task(void *param) {
    Product_t product = {0, 100};
    while (1) {
        product.id++;
        product.data += 10;
        USER_LOGD("生产者: 生产产品 ID %lu", product.id);
        if (!Queue_Send(product_queue, &product, MS_TO_TICKS(100))) {
            SYS_LOGW("生产者: 发送产品 ID %lu 失败 (队列满)", product.id);
        }
        Task_Delay(MS_TO_TICKS(2000));
    }
}

void consumer_task(void *param) {
    Product_t received_product;
    while (1) {
        USER_LOGD("消费者: 等待产品...");
        if (Queue_Receive(product_queue, &received_product, MY_RTOS_MAX_DELAY)) {
            USER_LOGD("消费者: 接收到产品 ID %lu, data %lu", received_product.id, received_product.data);
        }
    }
}

void inspector_task(void *param) {
    Product_t received_product;
    while (1) {
        USER_LOGD("质检员: 等待拦截产品...");
        if (Queue_Receive(product_queue, &received_product, MY_RTOS_MAX_DELAY)) {
            SYS_LOGW("质检员: 拦截到产品 ID %lu, data %lu", received_product.id, received_product.data);
        }
        Task_Delay(MS_TO_TICKS(5000)); // 质检员处理速度慢
    }
}

void recursive_test_task(void *param) {
    while (1) {
        SYS_LOGI("递归任务: 开始测试");
        Mutex_Lock_Recursive(recursive_lock);
        USER_LOGD("递归任务: 主循环加锁 (1层)");
        Task_Delay(MS_TO_TICKS(500));
        Mutex_Unlock_Recursive(recursive_lock);
        USER_LOGD("递归任务: 主循环解锁 (1层)");
        SYS_LOGI("递归任务: 已完全解锁，等待3秒");
        Task_Delay(MS_TO_TICKS(3000));
    }
}

void timer_test_task(void *param) {
#if (defined(MY_RTOS_TIMER_DEVICE_LIST))
    TimerHandle_dev_t user_timer = MyRTOS_Timer_GetHandle(TIMER_ID_USER_APP_TIMER);
    if (user_timer == NULL) {
        SYS_LOGE("Timer Test Task failed to get handle.");
        Task_Delete(NULL);
    }

    MyRTOS_Timer_Start(user_timer);
    while (1) {
        uint32_t count = MyRTOS_Timer_GetCount(user_timer);
        USER_LOGD("通用定时器测试: 当前计数值 = %lu", count);
        Task_Delay(MS_TO_TICKS(2000));
    }
#else
    SYS_LOGW("硬件定时器未在配置中定义，Timer Test任务退出。");
    Task_Delete(NULL);
#endif
}

void logger_task(void *param) {
    const char *taskName = (const char *) param;
    while (1) {
        USER_LOGD("%s: 正在等待打印机...", taskName);
        if (Semaphore_Take(logger_semaphore, MY_RTOS_MAX_DELAY)) {
            USER_LOGD("%s: [?] 获取到打印机, 开始打印 (耗时3秒)...", taskName);
            Task_Delay(MS_TO_TICKS(3000));
            USER_LOGD("%s: [↑] 打印完成, 释放打印机.", taskName);
            Semaphore_Give(logger_semaphore);
        }
        Task_Delay(MS_TO_TICKS(1000 + ((uint32_t)(uintptr_t)param % 3) * 500));
    }
}

void boot_task(void *param) {
    SYS_LOGI("Boot task running. Creating application tasks and services...");

#if (MY_RTOS_USE_SHELL == 1)
    MyRTOS_Shell_Start();
#endif

    // --- 互斥锁和协作任务 ---
    recursive_lock = Mutex_Create();
    a_task_h = Task_Create(a_task, "TaskA", 256, NULL, COLLABORATION_TASKS_PRIO);
    b_task_h = Task_Create(b_task, "TaskB", 256, NULL, COLLABORATION_TASKS_PRIO);
    d_task_h = Task_Create(d_task, "TaskD_Creator", 256, NULL, COLLABORATION_TASKS_PRIO);
    e_task_h = Task_Create(e_task, "LED_Blinky_PB2", 128, NULL, COLLABORATION_TASKS_PRIO);
    background_task_h = Task_Create(background_blinky_task, "BG_Blinky_PB0", 128, NULL, BACKGROUND_TASK_PRIO);
    high_prio_task_h = Task_Create(high_prio_task, "HighPrioTask", 256, NULL, HIGH_PRIO_TASK_PRIO);
    key_handler_task_h = Task_Create(key_handler_task, "KeyHandlerTask", 256, NULL, KEY_HANDLER_TASK_PRIO);
    recursive_task_h = Task_Create(recursive_test_task, "RecursiveTask", 256, NULL, COLLABORATION_TASKS_PRIO);
    timer_test_task_h = Task_Create(timer_test_task, "TimerTest", 256, NULL, TIMER_TEST_TASK_PRIO);

    // 启动任务A/B的协作循环
    Task_Notify(a_task_h);

    // --- 队列测试 ---
    product_queue = Queue_Create(3, sizeof(Product_t));
    if (product_queue) {
        consumer_task_h = Task_Create(consumer_task, "Consumer", 256, NULL, CONSUMER_PRIO);
        producer_task_h = Task_Create(producer_task, "Producer", 256, NULL, PRODUCER_PRIO);
        inspector_task_h = Task_Create(inspector_task, "Inspector", 256, NULL, INSPECTOR_PRIO);
    } else {
        SYS_LOGE("Product queue creation failed!");
    }

    // --- 软件定时器测试 ---
#if (MY_RTOS_USE_SOFTWARE_TIMERS == 1)
    perio_timer_h = Timer_Create(MS_TO_TICKS(10000), MS_TO_TICKS(10000), perio_timer_cb, NULL);
    single_timer_h = Timer_Create(MS_TO_TICKS(5000), 0, single_timer_cb, NULL);
    Timer_Start(perio_timer_h);
    Timer_Start(single_timer_h);
#endif

    // --- 信号量测试 ---
    logger_semaphore = Semaphore_Create(2, 2);
    if (logger_semaphore) {
        SYS_LOGI("创建3个日志任务来竞争2台打印机(信号量)...");
        logger_task1_h = Task_Create(logger_task, "LoggerTask1", 256, (void *) "打印任务1", LOGGER_TASK_PRIO);
        logger_task2_h = Task_Create(logger_task, "LoggerTask2", 256, (void *) "打印任务2", LOGGER_TASK_PRIO);
        logger_task3_h = Task_Create(logger_task, "LoggerTask3", 256, (void *) "打印任务3", LOGGER_TASK_PRIO);
    } else {
        SYS_LOGE("Logger semaphore creation failed!");
    }

    // --- 中断安全API测试 ---
    isr_semaphore = Semaphore_Create(1, 0); // 创建二进制信号量
    if (isr_semaphore) {
        isr_test_task_h = Task_Create(isr_test_task, "ISR_Test", 256, NULL, ISR_TEST_TASK_PRIO);
    } else {
        SYS_LOGE("ISR semaphore creation failed!");
    }

    SYS_LOGI("Boot task has finished. Deleting self.");
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
    int higherPriorityTaskWoken = 0;
    if (exti_interrupt_flag_get(EXTI_0) != RESET) {
        exti_interrupt_flag_clear(EXTI_0);

        if (key_handler_task_h != NULL) {
            Task_NotifyFromISR(key_handler_task_h, &higherPriorityTaskWoken);
        }
        if (isr_semaphore != NULL) {
            Semaphore_GiveFromISR(isr_semaphore, &higherPriorityTaskWoken);
        }

        if (higherPriorityTaskWoken) {
            MyRTOS_Port_YIELD();
        }
    }
}


int main(void) {
    MyRTOS_SystemInit();

#if (MY_RTOS_USE_SHELL == 1)
    MyRTOS_Platform_TerminalCommandsInit();
#endif

    key_exti_init();

    printf("\r\n\r\n");
    printf("===================================================\r\n");
    printf("=========          MyRTOS          =========\r\n");
    printf("|  Author: XiaoXiu\n");
    printf("|  Arch:   Cortex-M4F on GD32F4xx\n");
    printf("|  Build:  %s %s\r\n", __DATE__, __TIME__);
    printf("===================================================\r\n");
    SYS_LOGI("Press USER key to run Monitor snapshot");

    TaskHandle_t boot_task_h = Task_Create(boot_task, "BootTask", 1024, NULL, BOOT_TASK_PRIO);
    if (boot_task_h == NULL) {
        printf("FATAL: Failed to create Boot Task!\r\n");
        while (1);
    }

    SYS_LOGI("System Starting Scheduler...");
    Task_StartScheduler();

    while (1) {
    };
}
