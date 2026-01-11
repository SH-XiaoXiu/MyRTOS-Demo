/**
 * @brief MyRTOS 示例程序
 * @author XiaoXiu
 * @date  2025-08-31 2025-09-07更新
 */
#include <stdbool.h>
#include <string.h>
#include "MyRTOS.h"
#include "MyRTOS_IO.h"
#include "MyRTOS_Log.h"
#include "MyRTOS_Port.h"
#include "MyRTOS_Timer.h"
#include "gd32f4xx.h"
#include "gd32f4xx_exti.h"
#include "gd32f4xx_gpio.h"
#include "gd32f4xx_misc.h"
#include "gd32f4xx_rcu.h"
#include "gd32f4xx_syscfg.h"
#include "platform.h"
#include "platform_gd32_console.h"

#if MYRTOS_SERVICE_PROCESS_ENABLE == 1
#include "MyRTOS_Process.h"
#endif

#if MYRTOS_SERVICE_VTS_ENABLE == 1
#include "MyRTOS_VTS.h"
#endif

#include "init.h"

// 任务优先级定义
#define BACKGROUND_TASK_PRIO 1
#define CONSUMER_PRIO 2
#define PRINTER_TASK_PRIO 2
#define COLLABORATION_TASKS_PRIO 3
#define PRODUCER_PRIO 3
#define TIMER_TEST_TASK_PRIO 3
#define ISR_TEST_TASK_PRIO 4
#define HIGH_PRIO_TASK_PRIO 5
#define INTERRUPT_TASK_PRIO 6
#define INSPECTOR_PRIO 7
#define BOOTSTRAP_TASK_PRIO 10

// 队列消息结构体
typedef struct {
    uint32_t id;
    uint32_t data;
} Product_t;

// RTOS 内核对象句柄
static QueueHandle_t g_product_queue;
static MutexHandle_t g_recursive_lock;
static SemaphoreHandle_t g_printer_semaphore;
static SemaphoreHandle_t g_isr_semaphore;
static TimerHandle_t g_perio_timer_h, g_single_timer_h;

// 任务句柄
static TaskHandle_t g_producer_task_h, g_consumer_task_h, g_inspector_task_h;
static TaskHandle_t g_a_task_h, g_b_task_h, g_c_task_h, g_d_task_h;
static TaskHandle_t g_high_prio_task_h, g_interrupt_task_h, g_background_task_h;
static TaskHandle_t g_recursive_task_h, g_timer_test_task_h;
static TaskHandle_t g_printer_task1_h, g_printer_task2_h, g_printer_task3_h;
static TaskHandle_t g_isr_test_task_h;
static TaskHandle_t g_suspend_resume_test_task_h;
static TaskHandle_t g_target_task_h;
static TaskHandle_t g_bootstrap_task_h;


//可执行程序主函数
static int blinky_main(int argc, char *argv[]);

static int looper_main(int argc, char *argv[]);

static int hello_main(int argc, char *argv[]);

static int echo_main(int argc, char *argv[]);

static int spawner_main(int argc, char *argv[]);

// 可执行程序定义
const ProgramDefinition_t g_program_blinky = {
    .name = "blinky",
    .help = "LED闪烁进程 - 系统监控指示器",
    .main_func = blinky_main,
};

const ProgramDefinition_t g_program_looper = {
    .name = "looper", .help = "一个简单的后台循环程序.", .main_func = looper_main,
};
const ProgramDefinition_t g_program_echo = {
    .name = "echo", .help = "回显用户输入, 直到按下 'q'.", .main_func = echo_main,
};
const ProgramDefinition_t g_program_hello = {
    .name = "hello", .help = "打印 Hello World.", .main_func = hello_main,
};
const ProgramDefinition_t g_program_spawner = {
    .name = "spawner", .help = "测试进程创建工具. 用法: spawner <bound|detached> <prog>", .main_func = spawner_main,
};

// 外部程序定义（来自其他模块）
extern const ProgramDefinition_t g_program_init;
extern const ProgramDefinition_t g_program_shell;
extern const ProgramDefinition_t g_program_log;

//
// 私有函数声明
//

// 定时器回调函数
static void perio_timer_cb(TimerHandle_t timer);

static void single_timer_cb(TimerHandle_t timer);

// 任务函数
static void a_task(void *param);

static void b_task(void *param);

static void c_task(void *param);

static void d_task(void *param);

static void background_blinky_task(void *param);

static void high_prio_task(void *param);

static void interrupt_handler_task(void *param);

static void isr_test_task(void *param);

static void producer_task(void *param);

static void consumer_task(void *param);

static void inspector_task(void *param);

static void recursive_test_task(void *param);

static void timer_test_task(void *param);

static void printer_task(void *param);

static void suspend_resume_test_task(void *param);
static void target_task(void *param);
static void bootstrap_task(void *param);


// 初始化板级硬件
void Platform_BSP_Init_Hook(void) {
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_SYSCFG);
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_2);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_2);
    gpio_mode_set(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_0);
    syscfg_exti_line_config(EXTI_SOURCE_GPIOA, EXTI_SOURCE_PIN0);
    exti_init(EXTI_0, EXTI_INTERRUPT, EXTI_TRIG_FALLING);
    nvic_irq_enable(EXTI0_IRQn, 5, 0);
}

// 注册应用程序服务
void Platform_AppSetup_Hook(void) {
    // 注册可执行程序
#if MYRTOS_SERVICE_PROCESS_ENABLE == 1
    Process_RegisterProgram(&g_program_blinky);  // LED闪烁进程
    Process_RegisterProgram(&g_program_init);    // init进程
    Process_RegisterProgram(&g_program_shell);   // Shell程序
    Process_RegisterProgram(&g_program_looper);
    Process_RegisterProgram(&g_program_echo);
    Process_RegisterProgram(&g_program_hello);
    Process_RegisterProgram(&g_program_spawner); // 进程生成器测试工具
    Process_RegisterProgram(&g_program_log);     // Log监控程序
#endif
}

// 创建所有应用程序任务和内核对象
void Platform_CreateTasks_Hook(void) {
    // 创建软件定时器.
    g_single_timer_h = Timer_Create("单次定时器", MS_TO_TICKS(5000), 0, single_timer_cb, NULL);
    g_perio_timer_h = Timer_Create("周期定时器", MS_TO_TICKS(10000), 1, perio_timer_cb, NULL);
    if (g_single_timer_h) Timer_Start(g_single_timer_h, 0);
    if (g_perio_timer_h) Timer_Start(g_perio_timer_h, 0);

    // 创建队列及相关任务.
    g_product_queue = Queue_Create(3, sizeof(Product_t));
    if (g_product_queue) {
        g_consumer_task_h = Task_Create(consumer_task, "Consumer", 256, NULL, CONSUMER_PRIO);
        g_producer_task_h = Task_Create(producer_task, "Producer", 256, NULL, PRODUCER_PRIO);
        g_inspector_task_h = Task_Create(inspector_task, "Inspector", 256, NULL, INSPECTOR_PRIO);
    }

    // 创建信号量及相关任务.
    g_printer_semaphore = Semaphore_Create(2, 2);
    if (g_printer_semaphore) {
        g_printer_task1_h = Task_Create(printer_task, "PrinterTask1", 256, (void *) "PrinterTask1", PRINTER_TASK_PRIO);
        g_printer_task2_h = Task_Create(printer_task, "PrinterTask2", 256, (void *) "PrinterTask2", PRINTER_TASK_PRIO);
        g_printer_task3_h = Task_Create(printer_task, "PrinterTask3", 256, (void *) "PrinterTask3", PRINTER_TASK_PRIO);
    }

    // 创建用于 ISR 测试的信号量和任务.
    g_isr_semaphore = Semaphore_Create(10, 0);
    if (g_isr_semaphore) {
        g_isr_test_task_h = Task_Create(isr_test_task, "ISR_Test", 256, NULL, ISR_TEST_TASK_PRIO);
    }

    // 创建互斥锁及相关任务.
    g_recursive_lock = Mutex_Create();
    if (g_recursive_lock) {
        g_recursive_task_h = Task_Create(recursive_test_task, "RecursiveTask", 128, NULL, COLLABORATION_TASKS_PRIO);
    }

    // 创建其他演示任务.
    g_a_task_h = Task_Create(a_task, "TaskA", 128, NULL, COLLABORATION_TASKS_PRIO);
    g_b_task_h = Task_Create(b_task, "TaskB", 128, NULL, COLLABORATION_TASKS_PRIO);
    g_d_task_h = Task_Create(d_task, "TaskD_Creator", 256, NULL, COLLABORATION_TASKS_PRIO);
    g_background_task_h = Task_Create(background_blinky_task, "BG_Blinky_PB0", 64, NULL, BACKGROUND_TASK_PRIO);
    g_high_prio_task_h = Task_Create(high_prio_task, "HighPrioTask", 256, NULL, HIGH_PRIO_TASK_PRIO);
    g_interrupt_task_h = Task_Create(interrupt_handler_task, "KeyHandlerTask", 128, NULL, INTERRUPT_TASK_PRIO);
    g_timer_test_task_h = Task_Create(timer_test_task, "TimerTest", 512, NULL, TIMER_TEST_TASK_PRIO);
    g_target_task_h = Task_Create(target_task, "TargetTask", 256, NULL, 4);
    g_suspend_resume_test_task_h = Task_Create(suspend_resume_test_task, "SuspendTest", 256, NULL, 5);

    // 创建Bootstrap任务，它会在调度器启动后启动init进程
    g_bootstrap_task_h = Task_Create(bootstrap_task, "Bootstrap", 512, NULL, BOOTSTRAP_TASK_PRIO);
}

// EXTI0 中断服务程序.
void EXTI0_IRQHandler(void) {
    if (exti_interrupt_flag_get(EXTI_0) != RESET) {
        exti_interrupt_flag_clear(EXTI_0);
        int higherPriorityTaskWoken = 0;
        if (g_interrupt_task_h != NULL) {
            Task_NotifyFromISR(g_interrupt_task_h, &higherPriorityTaskWoken);
        }
        if (g_isr_semaphore != NULL) {
            Semaphore_GiveFromISR(g_isr_semaphore, &higherPriorityTaskWoken);
        }
        MyRTOS_Port_YieldFromISR(higherPriorityTaskWoken);
    }
}

void Platform_BSP_After_Hook(void) {
    StreamHandle_t console = Platform_Console_GetStream();
    if (console == NULL) {
        return;
    }

    // 打印平台硬件详细信息
    Stream_Printf(console, "[Platform]\r\n");
    Stream_Printf(console, "  CPU: GD32F407VGT6 (ARM Cortex-M4F, FPU enabled)\r\n");
    Stream_Printf(console, "  Clock: HXTAL 8MHz -> PLL %lu MHz\r\n", MYRTOS_CPU_CLOCK_HZ / 1000000);
    Stream_Printf(console, "    AHB=%lu MHz, APB1=%lu MHz, APB2=%lu MHz\r\n",
                 MYRTOS_CPU_CLOCK_HZ / 1000000,
                 MYRTOS_CPU_CLOCK_HZ / 4 / 1000000,
                 MYRTOS_CPU_CLOCK_HZ / 2 / 1000000);
    Stream_Printf(console, "  Console: USART%d @ %lu baud, 8N1\r\n",
                 PLATFORM_CONSOLE_USART_NUM,
                 PLATFORM_CONSOLE_BAUDRATE);
    Stream_Printf(console, "  Timer: TIM%d (high-resolution counter)\r\n", PLATFORM_HIRES_TIMER_NUM);
    Stream_Printf(console, "\r\n");
}

// 应用程序主入口.
int main(void) {
    Platform_Init();
    Platform_StartScheduler();
    return 0; // 不会执行到此处.
}

//
// 私有函数实现
//

// 周期性定时器回调.
static void perio_timer_cb(TimerHandle_t timer) {
    (void) timer;
    LOG_D("定时器回调", "周期性定时器(10秒)触发!");
}

// 一次性定时器回调.
static void single_timer_cb(TimerHandle_t timer) {
    (void) timer;
    LOG_D("定时器回调", "一次性定时器(5秒)触发!");
}

// 任务 A: 与任务 B 协作.
static void a_task(void *param) {
    (void) param;
    while (1) {
        Task_Wait();
        LOG_D("TaskA", "被唤醒, 开始工作...");
        for (int i = 1; i <= 5; ++i) {
            Task_Delay(MS_TO_TICKS(1000));
        }
        LOG_D("TaskA", "工作完成, 唤醒 Task B 并重新等待");
        Task_Notify(g_b_task_h);
    }
}

// 任务 B: 与任务 A 协作.
static void b_task(void *param) {
    (void) param;
    while (1) {
        Task_Wait();
        LOG_D("TaskB", "被唤醒, 开始工作...");
        for (int i = 1; i <= 3; ++i) {
            Task_Delay(MS_TO_TICKS(1000));
        }
        LOG_D("TaskB", "工作完成, 唤醒 Task A 并重新等待");
        Task_Notify(g_a_task_h);
    }
}

// 任务 C: 运行固定次数后自行删除.
static void c_task(void *param) {
    (void) param;
    for (int i = 1; i <= 5; ++i) {
        LOG_D("Task C", "正在运行, 第 %d 次", i);
        Task_Delay(MS_TO_TICKS(1000));
    }
    LOG_D("Task C", "运行5次后删除自己.");
    MyRTOS_Port_EnterCritical();
    g_c_task_h = NULL;
    MyRTOS_Port_ExitCritical();
    Task_Delete(NULL);
}

// 任务 D: 监控并按需重新创建任务 C.
static void d_task(void *param) {
    (void) param;
    while (1) {
        bool is_task_c_alive;
        MyRTOS_Port_EnterCritical();
        is_task_c_alive = (g_c_task_h != NULL);
        MyRTOS_Port_ExitCritical();
        if (!is_task_c_alive) {
            LOG_D("Task D", "检测到Task C不存在, 准备重新创建...");
            Task_Delay(MS_TO_TICKS(3000));
            g_c_task_h = Task_Create(c_task, "TaskC_dynamic", 1024, NULL, COLLABORATION_TASKS_PRIO);
        }
        Task_Delay(MS_TO_TICKS(1000));
    }
}

// 背景任务: LED闪烁 用来监控调度是否死掉
static void background_blinky_task(void *param) {
    (void) param;
    while (1) {
        gpio_bit_toggle(GPIOB, GPIO_PIN_2);
        Task_Delay(MS_TO_TICKS(1000));
    }
}

// 高优先级任务: 演示抢占.
static void high_prio_task(void *param) {
    (void) param;
    while (1) {
        Task_Delay(MS_TO_TICKS(5000));
        LOG_D("高优先级任务", "<<<<<<<<<< [抢占演示] >>>>>>>>>>");
    }
}

// 中断处理任务: 等待中断通知.
static void interrupt_handler_task(void *param) {
    (void) param;
    while (1) {
        Task_Wait();
        LOG_D("按键处理", "已被中断唤醒, 将唤醒A任务.");
        Task_Notify(g_a_task_h);
    }
}

// ISR 测试任务: 等待来自 ISR 的信号量.
static void isr_test_task(void *param) {
    (void) param;
    LOG_D("ISR测试", "启动并等待信号量...");
    while (1) {
        if (Semaphore_Take(g_isr_semaphore, MYRTOS_MAX_DELAY) == 1) {
            LOG_D("ISR测试", "成功从按键中断获取信号量!");
        }
    }
}

// 生产者任务: 向队列发送产品.
static void producer_task(void *param) {
    (void) param;
    Product_t product = {0, 100};
    while (1) {
        product.id++;
        product.data += 10;
        LOG_D("生产者", "生产产品 ID %lu", product.id);
        if (Queue_Send(g_product_queue, &product, MS_TO_TICKS(100)) != 1) {
            LOG_W("生产者", "队列已满, 发送产品 ID %lu 失败", product.id);
        }
        Task_Delay(MS_TO_TICKS(2000));
    }
}

// 消费者任务: 从队列接收产品.
static void consumer_task(void *param) {
    (void) param;
    Product_t received_product;
    while (1) {
        LOG_D("消费者", "等待产品...");
        if (Queue_Receive(g_product_queue, &received_product, MYRTOS_MAX_DELAY) == 1) {
            LOG_D("消费者", "接收到产品 ID %lu, 数据: %lu", received_product.id, received_product.data);
        }
    }
}

// 质检员任务: 以高优先级从队列拦截并销毁产品.
static void inspector_task(void *param) {
    (void) param;
    Product_t received_product;
    while (1) {
        LOG_D("质检员", "等待拦截产品...");
        if (Queue_Receive(g_product_queue, &received_product, MYRTOS_MAX_DELAY) == 1) {
            LOG_D("质检员", "拦截到产品 ID %lu, 销毁!", received_product.id);
        }
        Task_Delay(MS_TO_TICKS(5000));
    }
}

// 递归互斥锁测试任务.
static void recursive_test_task(void *param) {
    (void) param;
    while (1) {
        LOG_D("递归锁", "开始测试...");
        Mutex_Lock_Recursive(g_recursive_lock);
        LOG_D("递归锁", "主循环加锁 (第1层)");
        Task_Delay(MS_TO_TICKS(500));
        Mutex_Lock_Recursive(g_recursive_lock);
        LOG_D("递归锁", "嵌套加锁 (第2层)");
        Task_Delay(MS_TO_TICKS(500));
        Mutex_Unlock_Recursive(g_recursive_lock);
        LOG_D("递归锁", "嵌套解锁 (第2层)");
        Mutex_Unlock_Recursive(g_recursive_lock);
        LOG_D("递归锁", "主循环解锁 (第1层)");
        Task_Delay(MS_TO_TICKS(3000));
    }
}

// 高精度定时器值读取任务.
static void timer_test_task(void *param) {
    (void) param;
    while (1) {
        LOG_D("高精度时钟", "当前计数值 = %lu", Platform_Timer_GetHiresValue());
        Task_Delay(MS_TO_TICKS(2000));
    }
}

// 打印机任务: 演示使用信号量作为资源锁.
static void printer_task(void *param) {
    const char *taskName = (const char *) param;
    while (1) {
        LOG_D(taskName, "正在等待打印机...");
        if (Semaphore_Take(g_printer_semaphore, MYRTOS_MAX_DELAY) == 1) {
            LOG_D(taskName, "获取到打印机, 开始打印 (耗时3秒)...");
            Task_Delay(MS_TO_TICKS(3000));
            LOG_D(taskName, "打印完成, 释放打印机.");
            Semaphore_Give(g_printer_semaphore);
        }
        Task_Delay(MS_TO_TICKS(500 + (Task_GetId(Task_GetCurrentTaskHandle()) * 300)));
    }
}


static void target_task(void *param) {
    (void) param;
    int counter = 0;
    for (;;) {
        LOG_D("TargetTask", "I'm alive! Count = %d", counter++);
        Task_Delay(MS_TO_TICKS(1000));
    }
}

/**
 * @brief 测试任务, 用于控制目标任务的挂起和恢复.
 */
static void suspend_resume_test_task(void *param) {
    (void) param;

    LOG_I("SuspendTest", "Test started. Target task should be running now.");
    Task_Delay(MS_TO_TICKS(5000)); // 等待5秒, 观察目标任务运行

    if (g_target_task_h != NULL) {
        LOG_I("SuspendTest", "Suspending TargetTask...");
        Task_Suspend(g_target_task_h);
        LOG_I("SuspendTest", "TargetTask has been suspended. It should stop printing for 5 seconds.");
    }

    Task_Delay(MS_TO_TICKS(5000)); // 等待5秒, 确认目标任务已停止

    if (g_target_task_h != NULL) {
        LOG_I("SuspendTest", "Resuming TargetTask...");
        Task_Resume(g_target_task_h);
        LOG_I("SuspendTest", "TargetTask has been resumed. It should continue printing.");
    }

    Task_Delay(MS_TO_TICKS(5000)); // 再观察5秒, 确认目标任务已恢复

    // 清理测试任务
    if (g_target_task_h != NULL) {
        LOG_I("SuspendTest", "Test finished. Deleting TargetTask.");
        Task_Delete(g_target_task_h);
        g_target_task_h = NULL;
    }

    LOG_I("SuspendTest", "Deleting self.");
    Task_Delete(NULL); // 测试完成, 删除自身
}

// Bootstrap任务：在调度器启动后启动init进程
static void bootstrap_task(void *param) {
    (void)param;

    // 等待VTS服务就绪
    Task_Delay(MS_TO_TICKS(100));

    MyRTOS_printf("[Bootstrap] System is now in userspace\n");

#if MYRTOS_SERVICE_PROCESS_ENABLE == 1
    // 启动init进程（PID 1，绑定Bootstrap生命周期）
    MyRTOS_printf("[Bootstrap] Launching init process...\n");
    char *init_argv[] = {"init"};
    pid_t init_pid = Process_RunProgram("init", 1, init_argv, PROCESS_MODE_BOUND);

    if (init_pid <= 0) {
        MyRTOS_printf("[Bootstrap] FATAL: Failed to start init (PID 1)\n");
        while (1) Task_Delay(MS_TO_TICKS(1000));
    }

    MyRTOS_printf("[Bootstrap] init started (PID %d)\n", init_pid);

    // 获取init进程的stdin/stdout管道
    StreamHandle_t stdin_pipe = Process_GetFdHandleByPid(init_pid, STDIN_FILENO);
    StreamHandle_t stdout_pipe = Process_GetFdHandleByPid(init_pid, STDOUT_FILENO);

#if MYRTOS_SERVICE_VTS_ENABLE == 1
    // 切换VTS焦点到init进程
    if (stdin_pipe && stdout_pipe) {
        VTS_SetFocus(stdin_pipe, stdout_pipe);
    }

    // 等待init进程退出
    Task_WaitSignal(SIG_CHILD_EXIT | SIG_INTERRUPT | SIG_SUSPEND | SIG_BACKGROUND,
                    MYRTOS_MAX_DELAY, SIGNAL_WAIT_ANY | SIGNAL_CLEAR_ON_EXIT);

    // 恢复VTS焦点
    VTS_ReturnToRootFocus();
#else
    // 没有VTS，简单等待
    while (Process_GetState(init_pid) == PROCESS_STATE_RUNNING) {
        Task_Delay(MS_TO_TICKS(100));
    }
#endif

    MyRTOS_printf("[Bootstrap] init process exited (PID %d)\n", init_pid);
#endif

    // Bootstrap任务完成使命，删除自己
    Task_Delete(NULL);
}

//==============================================================================
// 测试程序实现
//==============================================================================

// blinky进程主函数 - LED闪烁，用来监控系统是否正常运行
static int blinky_main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    while (1) {
        gpio_bit_toggle(GPIOB, GPIO_PIN_2);

        // 等待1秒或收到中断信号
        uint32_t signals = Task_WaitSignal(SIG_INTERRUPT, MS_TO_TICKS(1000),
                                          SIGNAL_WAIT_ANY | SIGNAL_CLEAR_ON_EXIT);

        // 如果收到中断信号，关闭LED并退出
        if (signals & SIG_INTERRUPT) {
            gpio_bit_reset(GPIOB, GPIO_PIN_2);  // 关闭LED
            MyRTOS_printf("[blinky] LED OFF, exiting...\n");
            return 0;
        }
    }

    return 0;
}

// looper程序，在后台周期性打印消息
static int looper_main(int argc, char *argv[]) {
    (void) argc;
    (void) argv;
    int count = 0;
    const char *task_name = Task_GetName(NULL);
    for (;;) {
        // 这个printf的输出在后台模式下应该被丢弃
        MyRTOS_printf("[looper BG via printf] This message should be SILENT in background. Count: %d\n", count);
        // 这个LOG_I的输出应该总是可见的，通过VTS后台流
        LOG_I(task_name, "Hello from background via LOG! Count: %d", ++count);
        Task_Delay(MS_TO_TICKS(2000));
    }
    return 0;  // 实际永远不会到达
}

// hello程序，打印一条消息后退出
static int hello_main(int argc, char *argv[]) {
    (void) argc;
    (void) argv;
    MyRTOS_printf("你好世界 喵喵喵\n");
    Task_Delay(MS_TO_TICKS(1000));
    return 0;  // 正常退出
}

// echo程序，回显用户输入，直到输入q
static int echo_main(int argc, char *argv[]) {
    (void) argc;
    (void) argv;
    char ch;

    MyRTOS_printf("\n========================================\n");
    MyRTOS_printf("  Echo Program - Character Echo Test\n");
    MyRTOS_printf("========================================\n");
    MyRTOS_printf("Type any character to see it echoed back.\n");
    MyRTOS_printf("Press 'q' to quit.\n\n");

    for (;;) {
        ch = MyRTOS_getchar();

        if (ch == 'q' || ch == 'Q') {
            MyRTOS_printf("\nExiting echo program.\n");
            break;
        } else if (ch == '\r' || ch == '\n') {
            MyRTOS_printf("\n[Echo] Received <ENTER>\n");
        } else if (ch == '\t') {
            MyRTOS_printf("\n[Echo] Received <TAB>\n");
        } else if (ch == 0x1B) {  // ESC
            MyRTOS_printf("\n[Echo] Received <ESC>\n");
        } else if (ch >= 32 && ch <= 126) {  // 可打印字符
            MyRTOS_printf("\n[Echo] You typed: '%c' (ASCII: %d)\n", ch, (int)ch);
        } else {
            MyRTOS_printf("\n[Echo] Received non-printable char (ASCII: %d)\n", (int)ch);
        }
    }

    return 0;  // 用户按q退出
}

// spawner程序，测试进程生命周期绑定
static int spawner_main(int argc, char *argv[]) {
    if (argc < 3) {
        MyRTOS_printf("Usage: spawner <bound|detached> <program_name> [args...]\n");
        MyRTOS_printf("Example:\n");
        MyRTOS_printf("  spawner bound looper      - 创建绑定子进程\n");
        MyRTOS_printf("  spawner detached blinky   - 创建独立守护子进程\n");
        return -1;
    }

    const char *mode_str = argv[1];
    const char *prog_name = argv[2];
    ProcessMode_t mode;

    // 解析模式参数
    if (strcmp(mode_str, "bound") == 0) {
        mode = PROCESS_MODE_BOUND;
    } else if (strcmp(mode_str, "detached") == 0) {
        mode = PROCESS_MODE_DETACHED;
    } else {
        MyRTOS_printf("Error: Invalid mode '%s'. Use 'bound' or 'detached'.\n", mode_str);
        return -1;
    }

    // 准备子进程参数
    int child_argc = argc - 2;
    char **child_argv = &argv[2];

    // 创建子进程
    pid_t child_pid = Process_RunProgram(prog_name, child_argc, child_argv, mode);

    if (child_pid > 0) {
        MyRTOS_printf("Spawner [PID %d] created %s child '%s' [PID %d]\n",
                      getpid(), mode_str, prog_name, child_pid);

        // spawner进入等待循环，等待被杀死或收到信号
        MyRTOS_printf("Spawner waiting... (kill %d to test cascade termination)\n", getpid());

        while (1) {
            // 等待子进程退出信号或被杀死
            uint32_t signals = Task_WaitSignal(SIG_CHILD_EXIT, MS_TO_TICKS(5000),
                                               SIGNAL_WAIT_ANY | SIGNAL_CLEAR_ON_EXIT);

            if (signals & SIG_CHILD_EXIT) {
                MyRTOS_printf("Spawner: child process exited.\n");
                break;
            }

            // 每5秒打印一次心跳
            MyRTOS_printf("Spawner [PID %d] alive, child=%d (%s)\n",
                          getpid(), child_pid, mode_str);
        }
    } else {
        MyRTOS_printf("Error: Failed to spawn '%s'.\n", prog_name);
        return -1;
    }

    return 0;
}
