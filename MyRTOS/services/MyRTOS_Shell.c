#include "MyRTOS_Shell.h"

#if MYRTOS_SERVICE_SHELL_ENABLE == 1

#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include "MyRTOS.h"
#include "MyRTOS_IO.h"
#include "MyRTOS_Shell_Private.h"


static void parse_command(ShellInstance_t *shell) {
    char *p = shell->cmd_buffer;
    shell->argc = 0;
    while (*p && shell->argc < SHELL_MAX_ARGS) {
        while (*p && isspace((unsigned char) *p)) *p++ = '\0';
        if (*p == '\0') break;
        shell->argv[shell->argc++] = p;
        while (*p && !isspace((unsigned char) *p)) p++;
    }
}

static void dispatch_command(ShellInstance_t *shell) {
    if (shell->argc == 0) return;
    ShellCommandNode_t *node = shell->commands_head;
    while (node != NULL) {
        if (strcmp(shell->argv[0], node->name) == 0) {
            node->callback(shell, shell->argc, shell->argv);
            return;
        }
        node = node->next;
    }
    MyRTOS_printf("Command not found: %s\n", shell->argv[0]);
}


void Shell_Task(void *param) {
    ShellInstance_t *shell = (ShellInstance_t *) param;
    char ch;
    int len;
    MyRTOS_printf("\nMyRTOS Shell Initialized.\n");
    for (;;) {
        MyRTOS_printf("%s", shell->config.prompt);
        len = 0;
        memset(shell->cmd_buffer, 0, SHELL_CMD_BUFFER_SIZE);
        while (Stream_Read(Task_GetStdIn(NULL), &ch, 1, MYRTOS_MAX_DELAY) == 1) {
            if (ch == '\n') break;
            if (len < SHELL_CMD_BUFFER_SIZE - 1) {
                shell->cmd_buffer[len++] = ch;
            }
        }
        if (len > 0) {
            parse_command(shell);
            dispatch_command(shell);
        }
    }
}

ShellHandle_t Shell_Init(const ShellConfig_t *config) {
    if (!config) {
        return NULL;
    }

    ShellInstance_t *shell = (ShellInstance_t *) MyRTOS_Malloc(sizeof(ShellInstance_t));
    if (!shell) {
        return NULL;
    }

    memset(shell, 0, sizeof(ShellInstance_t));
    shell->config = *config;
    shell->commands_head = NULL; // 初始化一个空链表

    if (shell->config.prompt == NULL) {
        shell->config.prompt = "> ";
    }

    return shell;
}

TaskHandle_t Shell_Start(ShellHandle_t shell_h, const char *task_name, uint8_t task_priority,
                         uint16_t task_stack_size) {
    if (!shell_h) return NULL;
    return Task_Create(Shell_Task, task_name, task_stack_size, shell_h, task_priority);
}

int Shell_RegisterCommand(ShellHandle_t shell_h, const char *name, const char *help, ShellCommandCallback_t callback) {
    if (!shell_h || !name || !callback) {
        return -1;
    }
    ShellInstance_t *shell = (ShellInstance_t *) shell_h;
    ShellCommandNode_t *current = shell->commands_head;
    while (current != NULL) {
        if (strcmp(current->name, name) == 0) {
            return -2; // 命令已存在
        }
        current = current->next;
    }
    ShellCommandNode_t *new_node = (ShellCommandNode_t *) MyRTOS_Malloc(sizeof(ShellCommandNode_t));
    if (!new_node) {
        return -3; // 内存分配失败
    }
    new_node->name = name;
    new_node->help = help;
    new_node->callback = callback;
    new_node->next = shell->commands_head;
    shell->commands_head = new_node;
    return 0;
}

int Shell_UnregisterCommand(ShellHandle_t shell_h, const char *name) {
    if (!shell_h || !name) {
        return -1;
    }
    ShellInstance_t *shell = (ShellInstance_t *) shell_h;
    ShellCommandNode_t *current = shell->commands_head;
    ShellCommandNode_t *prev = NULL;
    while (current != NULL) {
        if (strcmp(current->name, name) == 0) {
            if (prev == NULL) {
                // 是头节点
                shell->commands_head = current->next;
            } else {
                prev->next = current->next;
            }
            MyRTOS_Free(current); // 释放节点内存
            return 0;
        }
        prev = current;
        current = current->next;
    }
    return -4; // 未找到命令
}

#endif
