/**
 * @file  shell_process_main.c
 * @brief Shell Process模式 - 作为用户进程运行
 */
#include "include/shell.h"
#include "MyRTOS.h"
#include "MyRTOS_IO.h"
#include "platform.h"

#if MYRTOS_SERVICE_PROCESS_ENABLE == 1
#include "MyRTOS_Process.h"
#endif

#if MYRTOS_SERVICE_VTS_ENABLE == 1
#include "MyRTOS_VTS.h"
#endif

// 清除当前行
static void clear_line(int length) {
    for (int i = 0; i < length; i++) {
        MyRTOS_printf("\b \b");
    }
}

// 替换当前行内容
static void replace_line(char *buffer, int *idx, const char *new_line) {
    // 清除当前行
    clear_line(*idx);

    // 显示新内容
    *idx = 0;
    while (new_line[*idx] && *idx < SHELL_MAX_LINE_LENGTH - 1) {
        buffer[*idx] = new_line[*idx];
        MyRTOS_putchar(buffer[*idx]);
        (*idx)++;
    }
    buffer[*idx] = '\0';
}

/**
 * @brief Shell Process主函数
 */
static int shell_process_main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    // 设置当前Task为VTS信号接收者，并切换到RAW模式
#if MYRTOS_SERVICE_VTS_ENABLE == 1
    VTS_SetSignalReceiver(Task_GetCurrentTaskHandle());
    VTS_SetTerminalMode(VTS_MODE_RAW);
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
    shell_register_sysinfo_commands(shell);

    // 主循环：读取命令并执行
    char line_buffer[SHELL_MAX_LINE_LENGTH];
    int escape_state = 0;  // 0=normal, 1=got ESC, 2=got ESC[

    while (1) {
        // 重置历史浏览状态
        shell_history_reset_browse(shell);

        // 显示提示符
        MyRTOS_printf("%s", shell_get_prompt(shell));

        // 读取一行输入
        int idx = 0;
        escape_state = 0;

        while (idx < SHELL_MAX_LINE_LENGTH - 1) {
            char ch = MyRTOS_getchar();

            // ANSI转义序列解析
            if (escape_state == 0 && ch == 0x1B) {  // ESC
                escape_state = 1;
                continue;
            } else if (escape_state == 1 && ch == '[') {  // ESC[
                escape_state = 2;
                continue;
            } else if (escape_state == 2) {
                // 处理箭头按键
                if (ch == 'A') {  // 上箭头
#if SHELL_HISTORY_SIZE > 0
                    // 第一次按上箭头时保存当前输入
                    line_buffer[idx] = '\0';
                    shell_history_save_temp(shell, line_buffer);

                    const char *prev = shell_get_prev_history(shell);
                    if (prev) {
                        replace_line(line_buffer, &idx, prev);
                    }
#endif
                } else if (ch == 'B') {  // 下箭头
#if SHELL_HISTORY_SIZE > 0
                    const char *next = shell_get_next_history(shell);
                    if (next) {
                        replace_line(line_buffer, &idx, next);
                    }
#endif
                }
                escape_state = 0;
                continue;
            }

            escape_state = 0;  // 重置转义状态

            // 处理控制字符 (RAW模式下需要自己处理)
            if (ch == 0x03) {  // Ctrl+C
                MyRTOS_printf("^C\n");
                idx = 0;
                line_buffer[0] = '\0';
                break;  // 回到提示符
            }

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
                if (idx < SHELL_MAX_LINE_LENGTH - 1) {
                    line_buffer[idx++] = ch;
                    MyRTOS_putchar(ch);
                }
            }
        }

        // 执行命令
        if (idx > 0) {
#if SHELL_HISTORY_SIZE > 0
            // 添加到历史记录
            shell_add_history(shell, line_buffer);
#endif
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
