/**
 * @file  shell_platform.c
 * @brief 平台命令（reboot）
 */
#include "shell.h"
#include "MyRTOS_IO.h"
#include "MyRTOS.h"
#include "platform.h"
#include <stdio.h>

// reboot 命令：重启系统
static int cmd_reboot(shell_handle_t shell, int argc, char *argv[]) {
    (void)shell;
    (void)argc;
    (void)argv;

    MyRTOS_printf("System rebooting...\n");
    Task_Delay(MS_TO_TICKS(100));

    // 调用平台层提供的重启接口
    Platform_Reboot();

    return 0;
}

void shell_register_platform_commands(shell_handle_t shell) {
    shell_register_command(shell, "reboot", "重启系统", cmd_reboot);
}
