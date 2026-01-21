/**
 * @brief MyRTOS GD32F407 Demo
 * @author XiaoXiu
 * @date  2025-08-31 2025-09-07更新
 *
 * 此 Demo 使用 MyRTOS Boot 模块进行标准启动
 */
#include <stdbool.h>
#include <string.h>
#include "MyRTOS.h"
#include "MyRTOS_Boot.h"
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

#if MYRTOS_SERVICE_PROCESS_ENABLE == 1
#include "MyRTOS_Process.h"
#endif

#if MYRTOS_SERVICE_VTS_ENABLE == 1
#include "MyRTOS_VTS.h"
#endif

#include "init.h"

// ============================================================================
//                           任务优先级定义
// ============================================================================
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

// ============================================================================
//                           数据结构定义
// ============================================================================
// 队列消息结构体
typedef struct {
    uint32_t id;
    uint32_t data;
} Product_t;

// ============================================================================
//                           全局变量
// ============================================================================
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

// ============================================================================
//                           可执行程序声明
// ============================================================================
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

// ============================================================================
//                           任务函数声明
// ============================================================================
static void perio_timer_cb(TimerHandle_t timer);
static void single_timer_cb(TimerHandle_t timer);
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

// ============================================================================
//                           Boot 回调函数
// ============================================================================

/**
 * @brief 内核初始化后回调
 *        用于初始化错误处理器等
 */
static void on_kernel_init(void) {
    // 初始化平台错误处理器
    extern void Platform_ErrorHandler_Init(void);
    Platform_ErrorHandler_Init();

    // 初始化控制台 OS 部分（信号量）
#if PLATFORM_USE_CONSOLE == 1
    Platform_Console_OSInit();
#endif
}

/**
 * @brief 服务初始化后回调
 *        用于注册程序、打印平台信息等
 */
static void on_services_init(void) {
    StreamHandle_t console = NULL;
#if PLATFORM_USE_CONSOLE == 1
    console = Platform_Console_GetStream();
#endif

    // 打印平台硬件详细信息
    if (console) {
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
#if PLATFORM_USE_HIRES_TIMER == 1
        Stream_Printf(console, "  Timer: TIM%d (high-resolution counter)\r\n", PLATFORM_HIRES_TIMER_NUM);
#endif
        Stream_Printf(console, "\r\n");
    }

    // 注册可执行程序
#if MYRTOS_SERVICE_PROCESS_ENABLE == 1
    Process_RegisterProgram(&g_program_blinky);
    Process_RegisterProgram(&g_program_init);
    Process_RegisterProgram(&g_program_shell);
    Process_RegisterProgram(&g_program_looper);
    Process_RegisterProgram(&g_program_echo);
    Process_RegisterProgram(&g_program_hello);
    Process_RegisterProgram(&g_program_spawner);
    Process_RegisterProgram(&g_program_log);
#endif
}

/**
 * @brief 创建所有应用程序任务
 */
static void create_tasks(void) {
    // 创建软件定时器
    g_single_timer_h = Timer_Create("单次定时器", MS_TO_TICKS(5000), 0, single_timer_cb, NULL);
    g_perio_timer_h = Timer_Create("周期定时器", MS_TO_TICKS(10000), 1, perio_timer_cb, NULL);
    if (g_single_timer_h) Timer_Start(g_single_timer_h, 0);
    if (g_perio_timer_h) Timer_Start(g_perio_timer_h, 0);

    // 创建队列及相关任务
    g_product_queue = Queue_Create(3, sizeof(Product_t));
    if (g_product_queue) {
        g_consumer_task_h = Task_Create(consumer_task, "Consumer", 256, NULL, CONSUMER_PRIO);
        g_producer_task_h = Task_Create(producer_task, "Producer", 256, NULL, PRODUCER_PRIO);
        g_inspector_task_h = Task_Create(inspector_task, "Inspector", 256, NULL, INSPECTOR_PRIO);
    }

    // 创建信号量及相关任务
    g_printer_semaphore = Semaphore_Create(2, 2);
    if (g_printer_semaphore) {
        g_printer_task1_h = Task_Create(printer_task, "PrinterTask1", 256, (void *) "PrinterTask1", PRINTER_TASK_PRIO);
        g_printer_task2_h = Task_Create(printer_task, "PrinterTask2", 256, (void *) "PrinterTask2", PRINTER_TASK_PRIO);
        g_printer_task3_h = Task_Create(printer_task, "PrinterTask3", 256, (void *) "PrinterTask3", PRINTER_TASK_PRIO);
    }

    // 创建用于 ISR 测试的信号量和任务
    g_isr_semaphore = Semaphore_Create(10, 0);
    if (g_isr_semaphore) {
        g_isr_test_task_h = Task_Create(isr_test_task, "ISR_Test", 256, NULL, ISR_TEST_TASK_PRIO);
    }

    // 创建互斥锁及相关任务
    g_recursive_lock = Mutex_Create();
    if (g_recursive_lock) {
        g_recursive_task_h = Task_Create(recursive_test_task, "RecursiveTask", 128, NULL, COLLABORATION_TASKS_PRIO);
    }

    // 创建其他演示任务
    g_a_task_h = Task_Create(a_task, "TaskA", 128, NULL, COLLABORATION_TASKS_PRIO);
    g_b_task_h = Task_Create(b_task, "TaskB", 128, NULL, COLLABORATION_TASKS_PRIO);
    g_d_task_h = Task_Create(d_task, "TaskD_Creator", 256, NULL, COLLABORATION_TASKS_PRIO);
    g_background_task_h = Task_Create(background_blinky_task, "BG_Blinky_PB0", 64, NULL, BACKGROUND_TASK_PRIO);
    g_high_prio_task_h = Task_Create(high_prio_task, "HighPrioTask", 256, NULL, HIGH_PRIO_TASK_PRIO);
    g_interrupt_task_h = Task_Create(interrupt_handler_task, "KeyHandlerTask", 128, NULL, INTERRUPT_TASK_PRIO);
    g_timer_test_task_h = Task_Create(timer_test_task, "TimerTest", 512, NULL, TIMER_TEST_TASK_PRIO);
    g_target_task_h = Task_Create(target_task, "TargetTask", 256, NULL, 4);
    g_suspend_resume_test_task_h = Task_Create(suspend_resume_test_task, "SuspendTest", 256, NULL, 5);

    // 创建 Bootstrap 任务
    g_bootstrap_task_h = Task_Create(bootstrap_task, "Bootstrap", 512, NULL, BOOTSTRAP_TASK_PRIO);
}

// ============================================================================
//                           平台钩子函数重写
// ============================================================================

// BSP 初始化钩子（配置 GPIO、外部中断等）
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

// 空闲任务钩子
void Platform_IdleTask_Hook(void *pv) {
    (void) pv;
    for (;;) {
        __WFI(); // 等待中断，进入低功耗模式
    }
}

// ============================================================================
//                           中断服务程序
// ============================================================================

// EXTI0 中断服务程序
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

// ============================================================================
//                           应用程序入口
// ============================================================================

int main(void) {
    // 1. 初始化平台硬件
    Platform_HwInit();

    // 2. 配置 Boot 参数
    BootConfig_t boot_config = {
#if MYRTOS_SERVICE_IO_ENABLE == 1
        .console_stream = Platform_Console_GetStream(),
#endif
#if MYRTOS_SERVICE_MONITOR_ENABLE == 1
        .get_hires_timer_value = Platform_Timer_GetHiresValue,
#endif
        .on_kernel_init = on_kernel_init,
        .on_services_init = on_services_init,
        .create_tasks = create_tasks,
        .idle_task = Platform_IdleTask_Hook,
    };

    // 3. 启动 MyRTOS（永不返回）
    MyRTOS_Boot(&boot_config);

    return 0; // 不会执行到此处
}

// ============================================================================
//                           定时器回调函数
// ============================================================================

static void perio_timer_cb(TimerHandle_t timer) {
    (void) timer;
    LOG_D("定时器回调", "周期性定时器(10秒)触发!");
}

static void single_timer_cb(TimerHandle_t timer) {
    (void) timer;
    LOG_D("定时器回调", "一次性定时器(5秒)触发!");
}

// ============================================================================
//                           任务函数实现
// ============================================================================

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

static void background_blinky_task(void *param) {
    (void) param;
    while (1) {
        gpio_bit_toggle(GPIOB, GPIO_PIN_2);
        Task_Delay(MS_TO_TICKS(1000));
    }
}

static void high_prio_task(void *param) {
    (void) param;
    while (1) {
        Task_Delay(MS_TO_TICKS(5000));
        LOG_D("高优先级任务", "<<<<<<<<<< [抢占演示] >>>>>>>>>>");
    }
}

static void interrupt_handler_task(void *param) {
    (void) param;
    while (1) {
        Task_Wait();
        LOG_D("按键处理", "已被中断唤醒, 将唤醒A任务.");
        Task_Notify(g_a_task_h);
    }
}

static void isr_test_task(void *param) {
    (void) param;
    LOG_D("ISR测试", "启动并等待信号量...");
    while (1) {
        if (Semaphore_Take(g_isr_semaphore, MYRTOS_MAX_DELAY) == 1) {
            LOG_D("ISR测试", "成功从按键中断获取信号量!");
        }
    }
}

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

static void timer_test_task(void *param) {
    (void) param;
    while (1) {
        LOG_D("高精度时钟", "当前计数值 = %lu", Platform_Timer_GetHiresValue());
        Task_Delay(MS_TO_TICKS(2000));
    }
}

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

static void suspend_resume_test_task(void *param) {
    (void) param;

    LOG_I("SuspendTest", "Test started. Target task should be running now.");
    Task_Delay(MS_TO_TICKS(5000));

    if (g_target_task_h != NULL) {
        LOG_I("SuspendTest", "Suspending TargetTask...");
        Task_Suspend(g_target_task_h);
        LOG_I("SuspendTest", "TargetTask has been suspended. It should stop printing for 5 seconds.");
    }

    Task_Delay(MS_TO_TICKS(5000));

    if (g_target_task_h != NULL) {
        LOG_I("SuspendTest", "Resuming TargetTask...");
        Task_Resume(g_target_task_h);
        LOG_I("SuspendTest", "TargetTask has been resumed. It should continue printing.");
    }

    Task_Delay(MS_TO_TICKS(5000));

    if (g_target_task_h != NULL) {
        LOG_I("SuspendTest", "Test finished. Deleting TargetTask.");
        Task_Delete(g_target_task_h);
        g_target_task_h = NULL;
    }

    LOG_I("SuspendTest", "Deleting self.");
    Task_Delete(NULL);
}

static void bootstrap_task(void *param) {
    (void)param;

    Task_Delay(MS_TO_TICKS(100));

    MyRTOS_printf("[Bootstrap] System is now in userspace\n");

#if MYRTOS_SERVICE_PROCESS_ENABLE == 1
    MyRTOS_printf("[Bootstrap] Launching init process...\n");
    char *init_argv[] = {"init"};
    pid_t init_pid = Process_RunProgram("init", 1, init_argv, PROCESS_MODE_BOUND);

    if (init_pid <= 0) {
        MyRTOS_printf("[Bootstrap] FATAL: Failed to start init (PID 1)\n");
        while (1) Task_Delay(MS_TO_TICKS(1000));
    }

    MyRTOS_printf("[Bootstrap] init started (PID %d)\n", init_pid);

    StreamHandle_t stdin_pipe = Process_GetFdHandleByPid(init_pid, STDIN_FILENO);
    StreamHandle_t stdout_pipe = Process_GetFdHandleByPid(init_pid, STDOUT_FILENO);

#if MYRTOS_SERVICE_VTS_ENABLE == 1
    if (stdin_pipe && stdout_pipe) {
        VTS_SetFocus(stdin_pipe, stdout_pipe);
    }

    Task_WaitSignal(SIG_CHILD_EXIT | SIG_INTERRUPT | SIG_SUSPEND | SIG_BACKGROUND,
                    MYRTOS_MAX_DELAY, SIGNAL_WAIT_ANY | SIGNAL_CLEAR_ON_EXIT);

    VTS_ReturnToRootFocus();
#else
    while (Process_GetState(init_pid) == PROCESS_STATE_RUNNING) {
        Task_Delay(MS_TO_TICKS(100));
    }
#endif

    MyRTOS_printf("[Bootstrap] init process exited (PID %d)\n", init_pid);
#endif

    Task_Delete(NULL);
}

// ============================================================================
//                           测试程序实现
// ============================================================================

static int blinky_main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    while (1) {
        gpio_bit_toggle(GPIOB, GPIO_PIN_2);

        uint32_t signals = Task_WaitSignal(SIG_INTERRUPT, MS_TO_TICKS(1000),
                                          SIGNAL_WAIT_ANY | SIGNAL_CLEAR_ON_EXIT);

        if (signals & SIG_INTERRUPT) {
            gpio_bit_reset(GPIOB, GPIO_PIN_2);
            MyRTOS_printf("[blinky] LED OFF, exiting...\n");
            return 0;
        }
    }

    return 0;
}

static int looper_main(int argc, char *argv[]) {
    (void) argc;
    (void) argv;
    int count = 0;
    const char *task_name = Task_GetName(NULL);
    for (;;) {
        MyRTOS_printf("[looper BG via printf] This message should be SILENT in background. Count: %d\n", count);
        LOG_I(task_name, "Hello from background via LOG! Count: %d", ++count);
        Task_Delay(MS_TO_TICKS(2000));
    }
    return 0;
}

static int hello_main(int argc, char *argv[]) {
    (void) argc;
    (void) argv;
    MyRTOS_printf("你好世界 喵喵喵\n");
    Task_Delay(MS_TO_TICKS(1000));
    return 0;
}

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
        } else if (ch == 0x1B) {
            MyRTOS_printf("\n[Echo] Received <ESC>\n");
        } else if (ch >= 32 && ch <= 126) {
            MyRTOS_printf("\n[Echo] You typed: '%c' (ASCII: %d)\n", ch, (int)ch);
        } else {
            MyRTOS_printf("\n[Echo] Received non-printable char (ASCII: %d)\n", (int)ch);
        }
    }

    return 0;
}

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

    if (strcmp(mode_str, "bound") == 0) {
        mode = PROCESS_MODE_BOUND;
    } else if (strcmp(mode_str, "detached") == 0) {
        mode = PROCESS_MODE_DETACHED;
    } else {
        MyRTOS_printf("Error: Invalid mode '%s'. Use 'bound' or 'detached'.\n", mode_str);
        return -1;
    }

    int child_argc = argc - 2;
    char **child_argv = &argv[2];

    pid_t child_pid = Process_RunProgram(prog_name, child_argc, child_argv, mode);

    if (child_pid > 0) {
        MyRTOS_printf("Spawner [PID %d] created %s child '%s' [PID %d]\n",
                      getpid(), mode_str, prog_name, child_pid);

        MyRTOS_printf("Spawner waiting... (kill %d to test cascade termination)\n", getpid());

        while (1) {
            uint32_t signals = Task_WaitSignal(SIG_CHILD_EXIT, MS_TO_TICKS(5000),
                                               SIGNAL_WAIT_ANY | SIGNAL_CLEAR_ON_EXIT);

            if (signals & SIG_CHILD_EXIT) {
                MyRTOS_printf("Spawner: child process exited.\n");
                break;
            }

            MyRTOS_printf("Spawner [PID %d] alive, child=%d (%s)\n",
                          getpid(), child_pid, mode_str);
        }
    } else {
        MyRTOS_printf("Error: Failed to spawn '%s'.\n", prog_name);
        return -1;
    }

    return 0;
}
