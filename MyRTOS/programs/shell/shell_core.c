/**
 * @file  shell_core.c
 * @brief Shell 命令解析引擎实现
 */
#include "shell.h"
#include "MyRTOS.h"
#include <string.h>
#include <ctype.h>

#define SHELL_MAX_ARGS 10
#define SHELL_CMD_BUFFER_SIZE 128

// 命令节点
typedef struct shell_command_node_t {
    char *name;
    char *help;
    shell_command_callback_t callback;
    struct shell_command_node_t *next;
} shell_command_node_t;

// Shell Core 实例
struct shell_core_t {
    char *prompt;
    shell_command_node_t *commands_head;

    // 解析缓冲区
    char cmd_buffer[SHELL_CMD_BUFFER_SIZE];
    int argc;
    char *argv[SHELL_MAX_ARGS];
};

// 内部函数
static void parse_command(shell_handle_t shell, const char *line);
static int dispatch_command(shell_handle_t shell);
static char *str_duplicate(const char *str);

// ============================================
// 公共 API 实现
// ============================================

shell_handle_t shell_create(const char *prompt) {
    shell_handle_t shell = (shell_handle_t)MyRTOS_Malloc(sizeof(struct shell_core_t));
    if (!shell) {
        return NULL;
    }

    memset(shell, 0, sizeof(struct shell_core_t));
    shell->prompt = prompt ? str_duplicate(prompt) : str_duplicate("> ");
    shell->commands_head = NULL;

    return shell;
}

void shell_destroy(shell_handle_t shell) {
    if (!shell) return;

    // 释放所有命令节点
    shell_command_node_t *node = shell->commands_head;
    while (node) {
        shell_command_node_t *next = node->next;
        MyRTOS_Free(node->name);
        MyRTOS_Free(node->help);
        MyRTOS_Free(node);
        node = next;
    }

    MyRTOS_Free(shell->prompt);
    MyRTOS_Free(shell);
}

int shell_register_command(shell_handle_t shell,
                          const char *name,
                          const char *help,
                          shell_command_callback_t callback) {
    if (!shell || !name || !callback) {
        return -1;
    }

    // 检查命令是否已存在
    shell_command_node_t *current = shell->commands_head;
    while (current) {
        if (strcmp(current->name, name) == 0) {
            return -2; // 命令已存在
        }
        current = current->next;
    }

    // 创建新节点
    shell_command_node_t *new_node = (shell_command_node_t*)MyRTOS_Malloc(sizeof(shell_command_node_t));
    if (!new_node) {
        return -1;
    }

    new_node->name = str_duplicate(name);
    new_node->help = help ? str_duplicate(help) : NULL;
    new_node->callback = callback;
    new_node->next = shell->commands_head;
    shell->commands_head = new_node;

    return 0;
}

int shell_unregister_command(shell_handle_t shell, const char *name) {
    if (!shell || !name) {
        return -1;
    }

    shell_command_node_t **pp = &shell->commands_head;
    while (*pp) {
        if (strcmp((*pp)->name, name) == 0) {
            shell_command_node_t *to_remove = *pp;
            *pp = to_remove->next;
            MyRTOS_Free(to_remove->name);
            MyRTOS_Free(to_remove->help);
            MyRTOS_Free(to_remove);
            return 0;
        }
        pp = &(*pp)->next;
    }

    return -1; // 未找到
}

int shell_execute_line(shell_handle_t shell, const char *line) {
    if (!shell || !line) {
        return -1;
    }

    // 解析命令行
    parse_command(shell, line);

    // 分发执行
    return dispatch_command(shell);
}

const char *shell_get_prompt(shell_handle_t shell) {
    return shell ? shell->prompt : "";
}

void shell_foreach_command(shell_handle_t shell, shell_command_visitor_t visitor, void *arg) {
    if (!shell || !visitor) return;

    shell_command_node_t *node = shell->commands_head;
    while (node) {
        if (!visitor(node->name, node->help, arg)) {
            break;
        }
        node = node->next;
    }
}

// ============================================
// 内部函数实现
// ============================================

static void parse_command(shell_handle_t shell, const char *line) {
    // 复制到缓冲区
    strncpy(shell->cmd_buffer, line, SHELL_CMD_BUFFER_SIZE - 1);
    shell->cmd_buffer[SHELL_CMD_BUFFER_SIZE - 1] = '\0';

    // 解析参数
    char *p = shell->cmd_buffer;
    shell->argc = 0;

    while (*p && shell->argc < SHELL_MAX_ARGS) {
        // 跳过空白
        while (*p && isspace((unsigned char)*p)) {
            *p++ = '\0';
        }

        if (*p == '\0') break;

        // 记录参数
        shell->argv[shell->argc++] = p;

        // 找到参数结尾
        while (*p && !isspace((unsigned char)*p)) {
            p++;
        }
    }
}

static int dispatch_command(shell_handle_t shell) {
    if (shell->argc == 0) {
        return 0; // 空命令
    }

    // 查找命令
    shell_command_node_t *node = shell->commands_head;
    while (node) {
        if (strcmp(shell->argv[0], node->name) == 0) {
            // 执行命令
            return node->callback(shell, shell->argc, shell->argv);
        }
        node = node->next;
    }

    // 命令未找到（由调用者处理输出）
    return -127; // 特殊返回值表示命令未找到
}

static char *str_duplicate(const char *str) {
    if (!str) return NULL;

    size_t len = strlen(str);
    char *dup = (char*)MyRTOS_Malloc(len + 1);
    if (dup) {
        strcpy(dup, str);
    }
    return dup;
}
