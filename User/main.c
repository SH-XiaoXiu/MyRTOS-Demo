/**
 * @file  main.c
 * @brief MyRTOS 示例程序
 * @author XiaoXiu
 * @date  2025-08-31
*/
#include "gd32f4xx_exti.h"
#include "gd32f4xx_gpio.h"
#include "gd32f4xx_misc.h"
#include "gd32f4xx_rcu.h"
#include "gd32f4xx_syscfg.h"
#include "platform.h"
#include "MyRTOS.h"
#include "MyRTOS_IO.h"
#include "MyRTOS_Log.h"
#include "MyRTOS_Port.h"
#include "MyRTOS_Timer.h"

/*===========================================================================*
 *                            任务优先级定义                                 *
 *===========================================================================*/
#define BACKGROUND_TASK_PRIO       1
#define CONSUMER_PRIO              2
#define PRINTER_TASK_PRIO          2
#define COLLABORATION_TASKS_PRIO   3
#define PRODUCER_PRIO              3
#define TIMER_TEST_TASK_PRIO       3
#define ISR_TEST_TASK_PRIO         4
#define HIGH_PRIO_TASK_PRIO        5
#define INTERRUPT_TASK_PRIO        6
#define INSPECTOR_PRIO             7

/*===========================================================================*
 *                          全局句柄和数据结构                               *
 *===========================================================================*/
static QueueHandle_t product_queue;
static MutexHandle_t recursive_lock;
static SemaphoreHandle_t printer_semaphore;
static SemaphoreHandle_t isr_semaphore;
static TimerHandle_t perio_timer_h, single_timer_h;

static TaskHandle_t producer_task_h, consumer_task_h, inspector_task_h;
static TaskHandle_t a_task_h, b_task_h, c_task_h, d_task_h;
static TaskHandle_t high_prio_task_h, interrupt_task_h, background_task_h;
static TaskHandle_t recursive_task_h, timer_test_task_h;
static TaskHandle_t printer_task1_h, printer_task2_h, printer_task3_h;
static TaskHandle_t isr_test_task_h;

typedef struct {
    uint32_t id;
    uint32_t data;
} Product_t;

/*===========================================================================*
 *                       任务和回调函数实现                                  *
 *===========================================================================*/
void perio_timer_cb(TimerHandle_t timer) {
    (void) timer;
    LOG_D("定时器回调", "周期性定时器(10秒)触发!");
}

void single_timer_cb(TimerHandle_t timer) {
    (void) timer;
    LOG_D("定时器回调", "一次性定时器(5秒)触发!");
}

void a_task(void *param) {
    (void) param;
    static uint16_t i = 0;
    while (1) {
        Task_Wait();
        LOG_D("Task A", "被唤醒，开始工作...");
        for (i = 1; i <= 5; ++i) {
            LOG_D("Task A", "正在运行, 第 %d 次", i);
            Task_Delay(MS_TO_TICKS(1000));
        }
        i = 0;
        LOG_D("Task A", "工作完成，唤醒 Task B 并重新等待");
        Task_Notify(b_task_h);
    }
}

void b_task(void *param) {
    (void) param;
    static uint16_t i = 0;
    while (1) {
        Task_Wait();
        LOG_D("Task B", "被唤醒，开始工作...");
        for (i = 1; i <= 3; ++i) {
            LOG_D("Task B", "正在运行, 第 %d 次", i);
            Task_Delay(MS_TO_TICKS(1000));
        }
        i = 0;
        LOG_D("Task B", "工作完成，唤醒 Task A 并重新等待");
        Task_Notify(a_task_h);
    }
}

void c_task(void *param) {
    (void) param;
    uint16_t index = 0;
    LOG_D("Task C", "已创建并开始运行.");
    while (1) {
        index++;
        LOG_D("Task C", "正在运行, 第 %d 次", index);
        if (index == 5) {
            LOG_D("Task C", "运行5次后删除自己.");
            MyRTOS_Port_EnterCritical();
            c_task_h = NULL; // 在临界区内安全地清除全局句柄
            MyRTOS_Port_ExitCritical();
            Task_Delete(NULL); // 删除自身
        }
        Task_Delay(MS_TO_TICKS(1000));
    }
}

void d_task(void *param) {
    (void) param;
    while (1) {
        int is_task_c_alive;
        MyRTOS_Port_EnterCritical();
        is_task_c_alive = (c_task_h != NULL);
        MyRTOS_Port_ExitCritical();

        if (!is_task_c_alive) {
            LOG_D("Task D", "检测到Task C不存在, 准备重新创建...");
            Task_Delay(MS_TO_TICKS(3000)); // 等待3秒再创建
            c_task_h = Task_Create(c_task, "TaskC_dynamic", 1024, NULL, COLLABORATION_TASKS_PRIO);
            if (c_task_h == NULL) {
                LOG_E("Task D", "创建Task C失败!");
            }
        }
        Task_Delay(MS_TO_TICKS(1000));
    }
}

void background_blinky_task(void *param) {
    (void) param;
    while (1) {
        gpio_bit_toggle(GPIOB, GPIO_PIN_2);
        Task_Delay(MS_TO_TICKS(1000));
    }
}

void high_prio_task(void *param) {
    (void) param;
    while (1) {
        Task_Delay(MS_TO_TICKS(5000));
        LOG_D("高优先级任务", "<<<<<<<<<< [抢占演示] >>>>>>>>>>");
    }
}

void interrupt_handler_task(void *param) {
    (void) param;
    // 这个任务的功能现在由平台层提供，但我们仍然保留它用于演示
    while (1) {
        Task_Wait();
        LOG_D("按键处理", "已被中断唤醒, 将唤醒A任务.");
        Task_Notify(a_task_h);
    }
}

void isr_test_task(void *param) {
    (void) param;
    LOG_D("ISR测试", "启动并等待信号量...");
    while (1) {
        if (Semaphore_Take(isr_semaphore, MYRTOS_MAX_DELAY) == 1) {
            LOG_D("ISR测试", "成功从按键中断获取信号量!");
        }
    }
}

void producer_task(void *param) {
    (void) param;
    Product_t product = {0, 100};
    while (1) {
        product.id++;
        product.data += 10;
        LOG_D("生产者", "生产产品 ID %lu", product.id);
        if (Queue_Send(product_queue, &product, MS_TO_TICKS(100)) == 1) {
            LOG_D("生产者", "产品 ID %lu 已发送", product.id);
        } else {
            LOG_D("生产者", "队列已满, 发送产品 ID %lu 失败", product.id);
        }
        Task_Delay(MS_TO_TICKS(2000));
    }
}

void consumer_task(void *param) {
    (void) param;
    Product_t received_product;
    while (1) {
        LOG_D("消费者", "等待产品...");
        if (Queue_Receive(product_queue, &received_product, MYRTOS_MAX_DELAY) == 1) {
            LOG_D("消费者", "接收到产品 ID %lu, 数据: %lu", received_product.id, received_product.data);
        }
    }
}

void inspector_task(void *param) {
    (void) param;
    Product_t received_product;
    while (1) {
        LOG_D("质检员", "等待拦截产品...");
        if (Queue_Receive(product_queue, &received_product, MYRTOS_MAX_DELAY) == 1) {
            LOG_D("质检员", "拦截到产品 ID %lu, 销毁!", received_product.id);
        }
        Task_Delay(MS_TO_TICKS(5000));
    }
}

void recursive_test_task(void *param) {
    (void) param;
    while (1) {
        LOG_D("递归锁", "开始测试...");
        Mutex_Lock_Recursive(recursive_lock);
        LOG_D("递归锁", "主循环加锁 (第1层)");
        Task_Delay(MS_TO_TICKS(500));

        Mutex_Lock_Recursive(recursive_lock);
        LOG_D("递归锁", "嵌套加锁 (第2层)");
        Task_Delay(MS_TO_TICKS(500));
        Mutex_Unlock_Recursive(recursive_lock);
        LOG_D("递归锁", "嵌套解锁 (第2层)");

        Mutex_Unlock_Recursive(recursive_lock);
        LOG_D("递归锁", "主循环解锁 (第1层)");
        LOG_D("递归锁", "测试完成, 等待3秒");
        Task_Delay(MS_TO_TICKS(3000));
    }
}

void timer_test_task(void *param) {
    (void) param;
    while (1) {
        uint32_t count = Platform_Timer_GetHiresValue();
        LOG_D("高精度时钟", "当前计数值 = %lu", count);
        Task_Delay(MS_TO_TICKS(2000));
    }
}

void printer_task(void *param) {
    const char *taskName = (const char *) param;
    while (1) {
        LOG_D(taskName, "正在等待打印机...");
        if (Semaphore_Take(printer_semaphore, MYRTOS_MAX_DELAY) == 1) {
            LOG_D(taskName, "获取到打印机, 开始打印 (耗时3秒)...");
            Task_Delay(MS_TO_TICKS(3000));
            LOG_D(taskName, "打印完成, 释放打印机.");
            Semaphore_Give(printer_semaphore);
        }
        Task_Delay(MS_TO_TICKS(500 + (Task_GetId(Task_GetCurrentTaskHandle()) * 300)));
    }
}

/*===========================================================================*
 *                        用户自定义Shell命令                                *
 *===========================================================================*/
int cmd_114514(ShellHandle_t shell_h, int argc, char *argv[]) {
    (void) argc;
    (void) argv;
    Stream_Printf(Shell_GetStream(shell_h), "嘿嘿嘿！！！\n");
    return 0;
}

const ShellCommand_t g_user_commands[] = {
    {"114514", "一个很臭的指令", cmd_114514},
};
const size_t g_user_command_count = sizeof(g_user_commands) / sizeof(g_user_commands[0]);

/*===========================================================================*
 *                   平台钩子函数 (Platform Hooks)                       *
 *===========================================================================*/

/**
 * @brief 初始化与板级相关的硬件 (BSP)。
 */
void Platform_BSP_Init_Hook(void) {
    // LED GPIO 初始化
    rcu_periph_clock_enable(RCU_GPIOB);
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_2);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_2);

    // 按键中断初始化 (PA0)
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_SYSCFG);
    gpio_mode_set(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_0);
    syscfg_exti_line_config(EXTI_SOURCE_GPIOA, EXTI_SOURCE_PIN0);
    exti_init(EXTI_0, EXTI_INTERRUPT, EXTI_TRIG_FALLING);
    nvic_irq_enable(EXTI0_IRQn, 5, 0);
}

/**
 * @brief 注册用户自己的Shell命令。
 */
void Platform_AppSetup_Hook(ShellHandle_t shell_h) {
    if (shell_h) {
        LOG_D("Hook", "Platform_AppSetup_Hook: Registering user commands...");
        Platform_RegisterShellCommands(g_user_commands, g_user_command_count);
    }
}

/**
 * @brief 创建所有的应用程序任务。
 */
void Platform_CreateTasks_Hook(void) {
    // LOG_D("Hook", "Platform_CreateTasks_Hook: Creating application tasks...");

    // --- 软件定时器测试 ---
    single_timer_h = Timer_Create("单次定时器", MS_TO_TICKS(5000), 0, single_timer_cb, NULL);
    perio_timer_h = Timer_Create("周期定时器", MS_TO_TICKS(10000), 1, perio_timer_cb, NULL);
    if (single_timer_h) Timer_Start(single_timer_h, 0);
    if (perio_timer_h) Timer_Start(perio_timer_h, 0);

    // --- 队列测试 ---
    product_queue = Queue_Create(3, sizeof(Product_t));
    if (product_queue) {
        consumer_task_h = Task_Create(consumer_task, "Consumer", 256, NULL, CONSUMER_PRIO);
        producer_task_h = Task_Create(producer_task, "Producer", 256, NULL, PRODUCER_PRIO);
        inspector_task_h = Task_Create(inspector_task, "Inspector", 256, NULL, INSPECTOR_PRIO);
    }

    // --- 信号量测试 (打印机) ---
    printer_semaphore = Semaphore_Create(2, 2);
    if (printer_semaphore) {
        printer_task1_h = Task_Create(printer_task, "PrinterTask1", 256, (void *) "PrinterTask1", PRINTER_TASK_PRIO);
        printer_task2_h = Task_Create(printer_task, "PrinterTask2", 256, (void *) "PrinterTask2", PRINTER_TASK_PRIO);
        printer_task3_h = Task_Create(printer_task, "PrinterTask3", 256, (void *) "PrinterTask3", PRINTER_TASK_PRIO);
    }

    // --- 中断安全API测试 ---
    isr_semaphore = Semaphore_Create(10, 0);
    if (isr_semaphore) {
        isr_test_task_h = Task_Create(isr_test_task, "ISR_Test", 256, NULL, ISR_TEST_TASK_PRIO);
    }

    // --- 互斥锁和协作任务 ---
    recursive_lock = Mutex_Create();
    if (recursive_lock) {
        recursive_task_h = Task_Create(recursive_test_task, "RecursiveTask", 128, NULL, COLLABORATION_TASKS_PRIO);
    }
    //
    a_task_h = Task_Create(a_task, "TaskA", 128, NULL, COLLABORATION_TASKS_PRIO);
    b_task_h = Task_Create(b_task, "TaskB", 128, NULL, COLLABORATION_TASKS_PRIO);
    // d_task_h = Task_Create(d_task, "TaskD_Creator", 256, NULL, COLLABORATION_TASKS_PRIO); //太他妈吃内存了 有时候创建的瞬间 run 指令跑不了 先注释了
    background_task_h = Task_Create(background_blinky_task, "BG_Blinky_PB0", 64, NULL, BACKGROUND_TASK_PRIO);
    high_prio_task_h = Task_Create(high_prio_task, "HighPrioTask", 256, NULL, HIGH_PRIO_TASK_PRIO);
    interrupt_task_h = Task_Create(interrupt_handler_task, "KeyHandlerTask", 128, NULL, INTERRUPT_TASK_PRIO);
    timer_test_task_h = Task_Create(timer_test_task, "TimerTest", 512, NULL, TIMER_TEST_TASK_PRIO);
}


/*===========================================================================*
 *                            中断服务程序                                   *
 *===========================================================================*/
void EXTI0_IRQHandler(void) {
    if (exti_interrupt_flag_get(EXTI_0) != RESET) {
        exti_interrupt_flag_clear(EXTI_0);
        int higherPriorityTaskWoken = 0;

        if (interrupt_task_h != NULL) {
            Task_NotifyFromISR(interrupt_task_h, &higherPriorityTaskWoken);
        }
        if (isr_semaphore != NULL) {
            Semaphore_GiveFromISR(isr_semaphore, &higherPriorityTaskWoken);
        }

        MyRTOS_Port_YieldFromISR(higherPriorityTaskWoken);
    }
}


/*===========================================================================*
 *                             主入口点                                      *
 *===========================================================================*/
int main(void) {
    //初始化平台层 (它会处理所有底层细节和RTOS服务)
    Platform_Init();

    //打印信息
    LOG_I("Main", "=========   MyRTOS 演示   =========");
    LOG_I("Main", "|  作者: XiaoXiu");
    LOG_I("Main", "|  按下用户按键可触发中断");
    LOG_I("Main", "===============================================");
    LOG_I("Main", "系统启动中...");

    //启动RTOS调度器 通过平台启动
    //自己裸机使用 Task_StartScheduler
    Platform_StartScheduler();
    return 0; // 永远不会执行到这里
}
