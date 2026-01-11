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

#if SHELL_HISTORY_SIZE > 0
    // 历史记录（环形缓冲区）
    char history[SHELL_HISTORY_SIZE][SHELL_MAX_LINE_LENGTH];
    int history_write_idx;  // 下一条历史写入位置
    int history_count;      // 当前历史条数
    int history_browse_idx; // 当前浏览位置（-1表示未浏览）
    char history_temp[SHELL_MAX_LINE_LENGTH]; // 临时保存当前正在编辑的命令
#endif
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

#if SHELL_HISTORY_SIZE > 0
    shell->history_write_idx = 0;
    shell->history_count = 0;
    shell->history_browse_idx = -1;
#endif

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

// ============================================
// 历史记录 API
// ============================================

#if SHELL_HISTORY_SIZE > 0
void shell_add_history(shell_handle_t shell, const char *line) {
    if (!shell || !line || line[0] == '\0') {
        return;
    }

    // 不记录重复的命令（与最近一条相同）
    if (shell->history_count > 0) {
        int last_idx = (shell->history_write_idx + SHELL_HISTORY_SIZE - 1) % SHELL_HISTORY_SIZE;
        if (strcmp(shell->history[last_idx], line) == 0) {
            return;
        }
    }

    // 写入历史记录
    strncpy(shell->history[shell->history_write_idx], line, SHELL_MAX_LINE_LENGTH - 1);
    shell->history[shell->history_write_idx][SHELL_MAX_LINE_LENGTH - 1] = '\0';

    // 更新写入位置
    shell->history_write_idx = (shell->history_write_idx + 1) % SHELL_HISTORY_SIZE;
    if (shell->history_count < SHELL_HISTORY_SIZE) {
        shell->history_count++;
    }

    // 重置浏览位置
    shell->history_browse_idx = -1;
}

void shell_history_reset_browse(shell_handle_t shell) {
    if (!shell) return;
    shell->history_browse_idx = -1;
}

void shell_history_save_temp(shell_handle_t shell, const char *line) {
    if (!shell || !line) return;
    strncpy(shell->history_temp, line, SHELL_MAX_LINE_LENGTH - 1);
    shell->history_temp[SHELL_MAX_LINE_LENGTH - 1] = '\0';
}

const char *shell_get_prev_history(shell_handle_t shell) {
    if (!shell || shell->history_count == 0) {
        return NULL;
    }

    if (shell->history_browse_idx == -1) {
        // 第一次按上箭头，从最新的历史开始
        shell->history_browse_idx = (shell->history_write_idx + SHELL_HISTORY_SIZE - 1) % SHELL_HISTORY_SIZE;
    } else {
        // 继续向上浏览
        int next_idx = (shell->history_browse_idx + SHELL_HISTORY_SIZE - 1) % SHELL_HISTORY_SIZE;

        // 检查是否到达最早的历史记录
        int oldest_idx = (shell->history_write_idx + SHELL_HISTORY_SIZE - shell->history_count) % SHELL_HISTORY_SIZE;
        if (shell->history_browse_idx == oldest_idx) {
            // 已经是最早的了，不再向上
            return shell->history[shell->history_browse_idx];
        }

        shell->history_browse_idx = next_idx;
    }

    return shell->history[shell->history_browse_idx];
}

const char *shell_get_next_history(shell_handle_t shell) {
    if (!shell || shell->history_browse_idx == -1) {
        return NULL;  // 没有在浏览历史
    }

    // 向下浏览
    int next_idx = (shell->history_browse_idx + 1) % SHELL_HISTORY_SIZE;

    // 检查是否回到当前输入
    if (next_idx == shell->history_write_idx) {
        // 回到当前输入
        shell->history_browse_idx = -1;
        return shell->history_temp;  // 返回临时保存的命令
    }

    shell->history_browse_idx = next_idx;
    return shell->history[shell->history_browse_idx];
}
#endif
