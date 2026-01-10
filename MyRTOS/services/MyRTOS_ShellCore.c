/**
 * @file  MyRTOS_ShellCore.c
 * @brief Shell 命令解析引擎实现
 */
#include "MyRTOS_ShellCore.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#define SHELL_MAX_ARGS 10
#define SHELL_CMD_BUFFER_SIZE 128

// 命令节点
typedef struct ShellCoreCommandNode_t {
    char *name;
    char *help;
    ShellCoreCommandCallback_t callback;
    struct ShellCoreCommandNode_t *next;
} ShellCoreCommandNode_t;

// Shell Core 实例
struct ShellCore_t {
    char *prompt;
    ShellCoreCommandNode_t *commands_head;

    // 解析缓冲区
    char cmd_buffer[SHELL_CMD_BUFFER_SIZE];
    int argc;
    char *argv[SHELL_MAX_ARGS];
};

// 内部函数
static void parse_command(ShellCoreHandle_t core, const char *line);
static int dispatch_command(ShellCoreHandle_t core);
static char *str_duplicate(const char *str);

// ============================================
// 公共 API 实现
// ============================================

ShellCoreHandle_t ShellCore_Create(const char *prompt) {
    ShellCoreHandle_t core = (ShellCoreHandle_t)malloc(sizeof(struct ShellCore_t));
    if (!core) {
        return NULL;
    }

    memset(core, 0, sizeof(struct ShellCore_t));
    core->prompt = prompt ? str_duplicate(prompt) : str_duplicate("> ");
    core->commands_head = NULL;

    return core;
}

void ShellCore_Destroy(ShellCoreHandle_t core) {
    if (!core) return;

    // 释放所有命令节点
    ShellCoreCommandNode_t *node = core->commands_head;
    while (node) {
        ShellCoreCommandNode_t *next = node->next;
        free(node->name);
        free(node->help);
        free(node);
        node = next;
    }

    free(core->prompt);
    free(core);
}

int ShellCore_RegisterCommand(ShellCoreHandle_t core,
                               const char *name,
                               const char *help,
                               ShellCoreCommandCallback_t callback) {
    if (!core || !name || !callback) {
        return -1;
    }

    // 检查命令是否已存在
    ShellCoreCommandNode_t *current = core->commands_head;
    while (current) {
        if (strcmp(current->name, name) == 0) {
            return -2; // 命令已存在
        }
        current = current->next;
    }

    // 创建新节点
    ShellCoreCommandNode_t *new_node = (ShellCoreCommandNode_t*)malloc(sizeof(ShellCoreCommandNode_t));
    if (!new_node) {
        return -1;
    }

    new_node->name = str_duplicate(name);
    new_node->help = help ? str_duplicate(help) : NULL;
    new_node->callback = callback;
    new_node->next = core->commands_head;
    core->commands_head = new_node;

    return 0;
}

int ShellCore_UnregisterCommand(ShellCoreHandle_t core, const char *name) {
    if (!core || !name) {
        return -1;
    }

    ShellCoreCommandNode_t **pp = &core->commands_head;
    while (*pp) {
        if (strcmp((*pp)->name, name) == 0) {
            ShellCoreCommandNode_t *to_remove = *pp;
            *pp = to_remove->next;
            free(to_remove->name);
            free(to_remove->help);
            free(to_remove);
            return 0;
        }
        pp = &(*pp)->next;
    }

    return -1; // 未找到
}

int ShellCore_ExecuteLine(ShellCoreHandle_t core, const char *line) {
    if (!core || !line) {
        return -1;
    }

    // 解析命令行
    parse_command(core, line);

    // 分发执行
    return dispatch_command(core);
}

const char *ShellCore_GetPrompt(ShellCoreHandle_t core) {
    return core ? core->prompt : "";
}

void ShellCore_ForEachCommand(ShellCoreHandle_t core, ShellCoreCommandVisitor_t visitor, void *arg) {
    if (!core || !visitor) return;

    ShellCoreCommandNode_t *node = core->commands_head;
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

static void parse_command(ShellCoreHandle_t core, const char *line) {
    // 复制到缓冲区
    strncpy(core->cmd_buffer, line, SHELL_CMD_BUFFER_SIZE - 1);
    core->cmd_buffer[SHELL_CMD_BUFFER_SIZE - 1] = '\0';

    // 解析参数
    char *p = core->cmd_buffer;
    core->argc = 0;

    while (*p && core->argc < SHELL_MAX_ARGS) {
        // 跳过空白
        while (*p && isspace((unsigned char)*p)) {
            *p++ = '\0';
        }

        if (*p == '\0') break;

        // 记录参数
        core->argv[core->argc++] = p;

        // 找到参数结尾
        while (*p && !isspace((unsigned char)*p)) {
            p++;
        }
    }
}

static int dispatch_command(ShellCoreHandle_t core) {
    if (core->argc == 0) {
        return 0; // 空命令
    }

    // 查找命令
    ShellCoreCommandNode_t *node = core->commands_head;
    while (node) {
        if (strcmp(core->argv[0], node->name) == 0) {
            // 执行命令
            return node->callback(core, core->argc, core->argv);
        }
        node = node->next;
    }

    // 命令未找到（由调用者处理输出）
    return -127; // 特殊返回值表示命令未找到
}

static char *str_duplicate(const char *str) {
    if (!str) return NULL;

    size_t len = strlen(str);
    char *dup = (char*)malloc(len + 1);
    if (dup) {
        strcpy(dup, str);
    }
    return dup;
}
