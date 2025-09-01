//
// Created by XiaoXiu on 8/30/2025.
//


#include "MyRTOS_Shell.h"


#if MYRTOS_SERVICE_SHELL_ENABLE == 1

#include "MyRTOS_IO.h"
#include "MyRTOS_Shell_Private.h"
#include "MyRTOS.h"
#include <string.h>
#include <ctype.h>



/*===========================================================================*
 *                              Private Functions                            *
 *===========================================================================*/

// 解析命令行字符串为 argc/argv
static void parse_command(ShellInstance_t *shell) {
    char *p = shell->cmd_buffer;
    shell->argc = 0;

    while (*p && shell->argc < SHELL_MAX_ARGS) {
        // 跳过前导空格
        while (*p && isspace((unsigned char) *p)) {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        // 记录参数起始位置
        shell->argv[shell->argc++] = p;
        // 寻找参数结束位置 (空格或字符串末尾)
        while (*p && !isspace((unsigned char) *p)) {
            p++;
        }
        // 如果不是字符串末尾，则用'\0'截断
        if (*p) {
            *p++ = '\0';
        }
    }
}

// 分发并执行命令
static void dispatch_command(ShellInstance_t *shell) {
    if (shell->argc == 0) {
        return;
    }
    ShellCommandNode_t *node = shell->commands_head;
    while (node != NULL) {
        if (strcmp(shell->argv[0], node->name) == 0) {
            node->callback(shell, shell->argc, shell->argv);
            return;
        }
        node = node->next;
    }

    Stream_Printf(shell->config.io_stream, "Command not found: %s\n", shell->argv[0]);
}

// Shell后台任务
static void Shell_Task(void *param) {
    ShellInstance_t *shell = (ShellInstance_t *) param;
    char ch;

    Stream_Printf(shell->config.io_stream, "\nMyRTOS Shell Initialized.\n");
    Stream_Write(shell->config.io_stream, shell->config.prompt, strlen(shell->config.prompt), MYRTOS_MAX_DELAY);

    for (;;) {
        // 阻塞读取一个字符
        if (Stream_Read(shell->config.io_stream, &ch, 1, MYRTOS_MAX_DELAY) == 1) {
            if (ch == '\r' || ch == '\n') {
                Stream_Printf(shell->config.io_stream, "\n"); // 回车换行

                // 解析并执行
                shell->cmd_buffer[shell->buffer_len] = '\0';
                parse_command(shell);
                dispatch_command(shell);

                // 重置
                shell->buffer_len = 0;
                Stream_Write(shell->config.io_stream, shell->config.prompt, strlen(shell->config.prompt),
                             MYRTOS_MAX_DELAY);
            } else if (ch == '\b' || ch == 127) {
                // 处理退格
                if (shell->buffer_len > 0) {
                    shell->buffer_len--;
                    // 终端回显: 退格-空格-退格
                    Stream_Write(shell->config.io_stream, "\b \b", 3, MYRTOS_MAX_DELAY);
                }
            } else if (isprint((unsigned char) ch) && shell->buffer_len < SHELL_CMD_BUFFER_SIZE - 1) {
                // 存入缓冲区并回显
                shell->cmd_buffer[shell->buffer_len++] = ch;
                Stream_Write(shell->config.io_stream, &ch, 1, MYRTOS_MAX_DELAY);
            }
        }
    }
}

/*===========================================================================*
 *                              Public API Implementation                    *
 *===========================================================================*/

ShellHandle_t Shell_Init(const ShellConfig_t *config) {
    if (!config || !config->io_stream) {
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

int Shell_Start(ShellHandle_t shell_h, const char *task_name, uint8_t task_priority, uint16_t task_stack_size) {
    if (!shell_h) {
        return -1;
    }
    TaskHandle_t task = Task_Create(Shell_Task, task_name, task_stack_size, shell_h, task_priority);
    return (task == NULL) ? -1 : 0;
}

StreamHandle_t Shell_GetStream(ShellHandle_t shell_h) {
    if (!shell_h) {
        return NULL;
    }
    return shell_h->config.io_stream;
}

int Shell_RegisterCommand(ShellHandle_t shell_h, const char *name, const char *help, ShellCommandCallback_t callback) {
    if (!shell_h || !name || !callback) {
        return -1;
    }
    ShellInstance_t *shell = (ShellInstance_t *)shell_h;

    // 检查命令是否已存在
    ShellCommandNode_t *current = shell->commands_head;
    while (current != NULL) {
        if (strcmp(current->name, name) == 0) {
            return -2; // 命令已存在
        }
        current = current->next;
    }

    // 创建新的命令节点
    ShellCommandNode_t *new_node = (ShellCommandNode_t *)MyRTOS_Malloc(sizeof(ShellCommandNode_t));
    if (!new_node) {
        return -3; // 内存分配失败
    }
    new_node->name = name;
    new_node->help = help;
    new_node->callback = callback;

    // 将新节点插入到链表头部 (最简单的方式)
    new_node->next = shell->commands_head;
    shell->commands_head = new_node;

    return 0;
}

int Shell_UnregisterCommand(ShellHandle_t shell_h, const char *name) {
    if (!shell_h || !name) {
        return -1;
    }
    ShellInstance_t *shell = (ShellInstance_t *)shell_h;

    ShellCommandNode_t *current = shell->commands_head;
    ShellCommandNode_t *prev = NULL;

    while (current != NULL) {
        if (strcmp(current->name, name) == 0) {
            if (prev == NULL) { // 是头节点
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
