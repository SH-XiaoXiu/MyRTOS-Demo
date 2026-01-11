/**
 * @file  shell_main.c
 * @brief Shell 主程序 - Demo模式作为Task运行
 */
#include "include/shell.h"
#include "MyRTOS.h"
#include "MyRTOS_IO.h"
#include "platform.h"

#if MYRTOS_SERVICE_VTS_ENABLE == 1
#include "MyRTOS_VTS.h"
#endif

static shell_handle_t g_shell = NULL;

/**
 * @brief Shell Task主函数
 */
static void shell_task_main(void *param) {
    (void)param;

    // 创建Shell实例
    g_shell = shell_create("MyRTOS> ");
    if (!g_shell) {
        MyRTOS_printf("Error: Failed to create shell instance\n");
        while (1) Task_Delay(MS_TO_TICKS(1000));
    }

    // 注册所有命令模块
    shell_register_builtin_commands(g_shell);
    shell_register_monitor_commands(g_shell);
    shell_register_log_commands(g_shell);
    shell_register_process_commands(g_shell);
    shell_register_platform_commands(g_shell);

    // 主循环：读取命令并执行
    char line_buffer[128];
    while (1) {
        // 显示提示符
        MyRTOS_printf("%s", shell_get_prompt(g_shell));

        // 读取一行输入
        int idx = 0;
        while (idx < (int)sizeof(line_buffer) - 1) {
            char ch = MyRTOS_getchar();

            if (ch == '\r' || ch == '\n') {
                MyRTOS_printf("\n");
                line_buffer[idx] = '\0';
                break;
            } else if (ch == '\b' || ch == 127) { // 退格
                if (idx > 0) {
                    idx--;
                    MyRTOS_printf("\b \b");
                }
            } else if (ch >= 32 && ch < 127) { // 可打印字符
                if (idx < (int)sizeof(line_buffer) - 1) {
                    line_buffer[idx++] = ch;
                    MyRTOS_putchar(ch);
                }
            }
        }

        // 执行命令
        if (idx > 0) {
            int ret = shell_execute_line(g_shell, line_buffer);
            if (ret == -127) {
                MyRTOS_printf("Unknown command. Type 'help' for available commands.\n");
            }
        }
    }
}

/**
 * @brief 创建Shell Task（Demo模式）
 * @return TaskHandle_t Shell任务句柄，失败返回NULL
 */
TaskHandle_t shell_create_task(void) {
    TaskHandle_t shell_task = Task_Create(
        shell_task_main,
        "Shell",
        4096,  // 栈大小
        NULL,
        4      // 优先级
    );

    if (!shell_task) {
        return NULL;
    }

#if MYRTOS_SERVICE_VTS_ENABLE == 1
    // 注册为VTS信号接收器（接收Ctrl+C/Z/B信号）
    VTS_SetSignalReceiver(shell_task);

    // 获取VTS的root输入输出流（已在Platform_Init中创建）
    StreamHandle_t vts_in = VTS_GetRootInputStream();
    StreamHandle_t vts_out = VTS_GetRootOutputStream();

    if (vts_in && vts_out) {
        // 将Shell Task的stdio连接到VTS root流
        Stream_SetTaskStdIn(shell_task, vts_in);
        Stream_SetTaskStdOut(shell_task, vts_out);
        Stream_SetTaskStdErr(shell_task, vts_out);
    }
#else
    // 如果没有VTS，直接使用系统标准流
    StreamHandle_t console = Platform_Console_GetStream();
    if (console) {
        Stream_SetTaskStdIn(shell_task, console);
        Stream_SetTaskStdOut(shell_task, console);
        Stream_SetTaskStdErr(shell_task, console);
    }
#endif

    return shell_task;
}
