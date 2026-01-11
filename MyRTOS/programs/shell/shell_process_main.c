/**
 * @file  shell_process_main.c
 * @brief Shell Process模式 - 作为用户进程运行
 */
#include "shell.h"
#include "MyRTOS.h"
#include "MyRTOS_IO.h"
#include "platform.h"

#if MYRTOS_SERVICE_PROCESS_ENABLE == 1
#include "MyRTOS_Process.h"
#endif

#if MYRTOS_SERVICE_VTS_ENABLE == 1
#include "MyRTOS_VTS.h"
#endif

/**
 * @brief Shell Process主函数
 */
static int shell_process_main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    // 设置当前Task为VTS信号接收者
#if MYRTOS_SERVICE_VTS_ENABLE == 1
    VTS_SetSignalReceiver(Task_GetCurrentTaskHandle());
#endif

    // 创建Shell实例
    shell_handle_t shell = shell_create("MyRTOS> ");
    if (!shell) {
        MyRTOS_printf("Error: Failed to create shell instance\n");
        return -1;
    }

    // 注册所有命令模块
    shell_register_builtin_commands(shell);
    shell_register_monitor_commands(shell);
    shell_register_log_commands(shell);
    shell_register_process_commands(shell);
    shell_register_platform_commands(shell);

    // 主循环：读取命令并执行
    char line_buffer[128];
    while (1) {
        // 显示提示符
        MyRTOS_printf("%s", shell_get_prompt(shell));

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
            int ret = shell_execute_line(shell, line_buffer);
            if (ret == -127) {
                MyRTOS_printf("Unknown command. Type 'help' for available commands.\n");
            }
        }
    }

    // 不会到达
    shell_destroy(shell);
    return 0;
}

/**
 * @brief Shell程序定义（供Process系统注册）
 */
const ProgramDefinition_t g_program_shell = {
    .name = "shell",
    .help = "MyRTOS交互式Shell",
    .main_func = shell_process_main,
};
