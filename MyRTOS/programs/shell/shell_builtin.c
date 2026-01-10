/**
 * @file  shell_builtin.c
 * @brief Shell 内置命令（help）
 */
#include "shell.h"
#include "MyRTOS_IO.h"
#include <stdio.h>

// 打印命令的visitor函数
static bool print_command_visitor(const char *name, const char *help, void *arg) {
    (void)arg;
    MyRTOS_printf("  %-12s - %s\n", name, help);
    return true;
}

// help 命令：显示所有可用命令
static int cmd_help(shell_handle_t shell, int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    MyRTOS_printf("Available Commands:\n");
    shell_foreach_command(shell, print_command_visitor, NULL);

    return 0;
}

void shell_register_builtin_commands(shell_handle_t shell) {
    shell_register_command(shell, "help", "显示所有可用命令", cmd_help);
}
