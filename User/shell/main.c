/**
 * @file  main.c
 * @brief Shell Process测试 - 测试Process机制的可行性和稳定性
 * @note  通过Shell作为载体，验证Process创建、调度、信号处理、前后台切换等机制
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

// 任务优先级定义
#define BACKGROUND_TASK_PRIO 1

// RTOS 内核对象句柄
static TaskHandle_t g_background_task_h;

//==============================================================================
// 任务函数声明
//==============================================================================
static void background_blinky_task(void *param);

//==============================================================================
// 测试用可执行程序定义
//==============================================================================

static int looper_main(int argc, char *argv[]);
static int hello_main(int argc, char *argv[]);
static int echo_main(int argc, char *argv[]);

const ProgramDefinition_t g_program_looper = {
    .name = "looper", .help = "一个简单的后台循环程序.", .main_func = looper_main,
};
const ProgramDefinition_t g_program_echo = {
    .name = "echo", .help = "回显用户输入, 直到按下 'q'.", .main_func = echo_main,
};
const ProgramDefinition_t g_program_hello = {
    .name = "hello", .help = "打印 Hello World.", .main_func = hello_main,
};

// Shell程序定义（将在init进程中启动）
extern const ProgramDefinition_t g_program_shell;

//==============================================================================
// 平台钩子函数
//==============================================================================

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

void Platform_AppSetup_Hook(void) {
    // 注册可执行程序
#if MYRTOS_SERVICE_PROCESS_ENABLE == 1
    Process_RegisterProgram(&g_program_init);    // init进程（必须第一个注册）
    Process_RegisterProgram(&g_program_shell);   // Shell程序
    Process_RegisterProgram(&g_program_looper);
    Process_RegisterProgram(&g_program_echo);
    Process_RegisterProgram(&g_program_hello);
#endif
}

// init进程主函数 - 系统启动后的第一个用户进程
static int init_process_main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    MyRTOS_printf("\n");
    MyRTOS_printf("=== init process started ===\n");
    MyRTOS_printf("Starting Shell...\n");

    // 启动Shell进程（前台模式）
    char *shell_argv[] = {"shell"};
    pid_t shell_pid = Process_RunProgram("shell", 1, shell_argv, PROCESS_MODE_FOREGROUND);

    if (shell_pid > 0) {
        MyRTOS_printf("Shell started with PID %d\n", shell_pid);
    } else {
        MyRTOS_printf("Failed to start Shell!\n");
    }

    // init进程通常不会退出，在这里等待Shell退出
    for (;;) {
        Task_Delay(MS_TO_TICKS(1000));
    }

    return 0;
}

// init进程定义
const ProgramDefinition_t g_program_init = {
    .name = "init",
    .help = "Init process - 系统启动进程",
    .main_func = init_process_main,
};

void Platform_CreateTasks_Hook(void) {
    // 创建后台LED闪烁任务（监控调度器是否正常）
    g_background_task_h = Task_Create(background_blinky_task, "BG_Blinky", 64, NULL, BACKGROUND_TASK_PRIO);

#if MYRTOS_SERVICE_PROCESS_ENABLE == 1
    // 启动init进程（系统的第一个用户进程）
    char *init_argv[] = {"init"};
    pid_t init_pid = Process_RunProgram("init", 1, init_argv, PROCESS_MODE_FOREGROUND);

    if (init_pid <= 0) {
        MyRTOS_printf("FATAL: Failed to start init process!\n");
        while (1) Task_Delay(MS_TO_TICKS(1000));
    }
#endif
}

void EXTI0_IRQHandler(void) {
    if (exti_interrupt_flag_get(EXTI_0) != RESET) {
        exti_interrupt_flag_clear(EXTI_0);
        // 中断处理（如果需要）
    }
}

void Platform_BSP_After_Hook(void) {
    StreamHandle_t console = Platform_Console_GetStream();
    if (console == NULL) {
        return;
    }
    Stream_Printf(console, "\r\n\r\n");
    Stream_Printf(console, "==================================================\r\n");
    Stream_Printf(console, "*            MyRTOS Shell Process 测试           *\r\n");
    Stream_Printf(console, "*------------------------------------------------*\r\n");
    Stream_Printf(console, "* 测试目标: Process机制的可行性和稳定性          *\r\n");
    Stream_Printf(console, "* 测试内容: 进程创建/销毁/前后台切换/信号处理    *\r\n");
    Stream_Printf(console, "* 作者: XiaoXiu                                  *\r\n");
    Stream_Printf(console, "* 构建于: %s %s                *\r\n", __DATE__, __TIME__);
    Stream_Printf(console, "==================================================\r\n");
    Stream_Printf(console, "系统启动中...\r\n");
}

int main(void) {
    Platform_Init();
    Platform_StartScheduler();
    return 0;
}

//==============================================================================
// 任务函数实现
//==============================================================================

// 背景任务: LED闪烁 用来监控调度是否死掉
static void background_blinky_task(void *param) {
    (void) param;
    while (1) {
        gpio_bit_toggle(GPIOB, GPIO_PIN_2);
        Task_Delay(MS_TO_TICKS(1000));
    }
}

//==============================================================================
// 测试程序实现
//==============================================================================

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
    MyRTOS_printf("Echo program started. Type characters to echo.\n");
    MyRTOS_printf("Press 'q' to quit.\n");
    for (;;) {
        ch = MyRTOS_getchar();
        if (ch == 'q') {
            MyRTOS_printf("\nExiting echo program.\n");
            break;
        } else if (ch == '\r' || ch == '\n') {
            MyRTOS_printf("\n");
        } else {
            MyRTOS_putchar(ch);
        }
    }
    return 0;  // 用户按q退出
}
