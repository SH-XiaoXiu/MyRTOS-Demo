/**
 * @file  main.c
 * @brief Shell Process测试 - 测试Process机制的可行性和稳定性
 * @note  通过Shell作为载体，验证Process创建、调度、信号处理、前后台切换等机制
 */
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
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

// 任务优先级定义
#define BACKGROUND_TASK_PRIO 1
#define BOOTSTRAP_TASK_PRIO 10

// RTOS 内核对象句柄
static TaskHandle_t g_bootstrap_task_h;

//==============================================================================
// 任务函数声明
//==============================================================================
static void bootstrap_task(void *param);

//==============================================================================
// 测试用可执行程序定义
//==============================================================================

static int blinky_main(int argc, char *argv[]);
static int init_process_main(int argc, char *argv[]);
static int looper_main(int argc, char *argv[]);
static int hello_main(int argc, char *argv[]);
static int echo_main(int argc, char *argv[]);
static int spawner_main(int argc, char *argv[]);

// blinky进程定义 - LED闪烁
const ProgramDefinition_t g_program_blinky = {
    .name = "blinky",
    .help = "LED闪烁进程 - 系统监控指示器",
    .main_func = blinky_main,
};

// init进程定义
const ProgramDefinition_t g_program_init = {
    .name = "init",
    .help = "Init process - 系统启动进程",
    .main_func = init_process_main,
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
    Process_RegisterProgram(&g_program_blinky);  // LED闪烁进程
    Process_RegisterProgram(&g_program_init);    // init进程
    Process_RegisterProgram(&g_program_shell);   // Shell程序
    Process_RegisterProgram(&g_program_looper);
    Process_RegisterProgram(&g_program_echo);
    Process_RegisterProgram(&g_program_hello);
    Process_RegisterProgram(&g_program_spawner); // 进程生成器测试工具
#endif
}

// init进程主函数 - 系统启动后的第一个用户进程
static int init_process_main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    // 等待父任务切换VTS焦点（避免stdout管道阻塞）
    Task_Delay(MS_TO_TICKS(50));

    MyRTOS_printf("\n");
    MyRTOS_printf("=== MyRTOS Shell Environment ===\n");
    MyRTOS_printf("Starting Shell...\n");
    char *shell_argv[] = {"shell"};
    pid_t shell_pid = Process_RunProgram("shell", 1, shell_argv, PROCESS_MODE_BOUND);

    if (shell_pid <= 0) {
        MyRTOS_printf("Failed to start Shell!\n");
        return -1;
    }

    // 获取shell进程的stdin/stdout管道
    StreamHandle_t stdin_pipe = Process_GetFdHandleByPid(shell_pid, STDIN_FILENO);
    StreamHandle_t stdout_pipe = Process_GetFdHandleByPid(shell_pid, STDOUT_FILENO);

#if MYRTOS_SERVICE_VTS_ENABLE == 1
    // 切换VTS焦点到shell进程
    if (stdin_pipe && stdout_pipe) {
        VTS_SetFocus(stdin_pipe, stdout_pipe);
    }

    // 等待shell进程退出或信号
    uint32_t received_signals = Task_WaitSignal(
        SIG_CHILD_EXIT | SIG_INTERRUPT | SIG_SUSPEND | SIG_BACKGROUND,
        MYRTOS_MAX_DELAY, SIGNAL_WAIT_ANY | SIGNAL_CLEAR_ON_EXIT);

    // 恢复VTS焦点
    VTS_ReturnToRootFocus();

    if (received_signals & SIG_CHILD_EXIT) {
        MyRTOS_printf("\nShell process (PID %d) exited normally.\n", shell_pid);
    }
#else
    // 没有VTS，简单等待shell退出
    while (Process_GetState(shell_pid) == PROCESS_STATE_RUNNING) {
        Task_Delay(MS_TO_TICKS(100));
    }
    MyRTOS_printf("Shell process (PID %d) exited.\n", shell_pid);
#endif

    MyRTOS_printf("init process exiting.\n");
    return 0;
}

// Bootstrap任务：在调度器启动后启动init进程
static void bootstrap_task(void *param) {
    (void)param;

    // 等待VTS服务就绪
    Task_Delay(MS_TO_TICKS(100));

#if MYRTOS_SERVICE_PROCESS_ENABLE == 1
    // 启动init进程（PID 1，绑定Bootstrap生命周期）
    char *init_argv[] = {"init"};
    pid_t init_pid = Process_RunProgram("init", 1, init_argv, PROCESS_MODE_BOUND);

    if (init_pid <= 0) {
        MyRTOS_printf("FATAL: Failed to start init process!\n");
        while (1) Task_Delay(MS_TO_TICKS(1000));
    }

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

    MyRTOS_printf("init process (PID %d) exited.\n", init_pid);
#endif

    // Bootstrap任务完成使命，删除自己
    Task_Delete(NULL);
}

void Platform_CreateTasks_Hook(void) {
    // 创建Bootstrap任务，它会在调度器启动后启动blinky和init进程
    g_bootstrap_task_h = Task_Create(bootstrap_task, "Bootstrap", 512, NULL, BOOTSTRAP_TASK_PRIO);
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

// blinky进程主函数 - LED闪烁，用来监控系统是否正常运行
static int blinky_main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    while (1) {
        gpio_bit_toggle(GPIOB, GPIO_PIN_2);
        Task_Delay(MS_TO_TICKS(1000));
    }

    return 0;  // 永远不会到达
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
