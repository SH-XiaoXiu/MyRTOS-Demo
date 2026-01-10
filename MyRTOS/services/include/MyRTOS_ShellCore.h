/**
 * @file  MyRTOS_ShellCore.h
 * @brief Shell 命令解析引擎（库）
 * @details ShellCore 是纯粹的命令管理和分发引擎，不包含任何 Task 创建逻辑。
 *          可以被 Shell Task（旧）或 Shell Process（新）使用。
 */
#ifndef MYRTOS_SHELL_CORE_H
#define MYRTOS_SHELL_CORE_H

#include <stdint.h>
#include <stdbool.h>

// 前向声明
typedef struct ShellCore_t *ShellCoreHandle_t;

/**
 * @brief Shell 命令回调函数类型
 * @param core Shell Core 句柄
 * @param argc 参数数量
 * @param argv 参数数组
 * @return 0 表示成功，非 0 表示失败
 */
typedef int (*ShellCoreCommandCallback_t)(ShellCoreHandle_t core, int argc, char *argv[]);

/**
 * @brief 创建 Shell Core 实例
 * @param prompt 命令提示符（如 "MyRTOS> "）
 * @return Shell Core 句柄，失败返回 NULL
 */
ShellCoreHandle_t ShellCore_Create(const char *prompt);

/**
 * @brief 销毁 Shell Core 实例
 * @param core Shell Core 句柄
 */
void ShellCore_Destroy(ShellCoreHandle_t core);

/**
 * @brief 注册命令
 * @param core Shell Core 句柄
 * @param name 命令名称
 * @param help 帮助信息
 * @param callback 命令回调函数
 * @return 0 成功，-1 失败，-2 命令已存在
 */
int ShellCore_RegisterCommand(ShellCoreHandle_t core,
                               const char *name,
                               const char *help,
                               ShellCoreCommandCallback_t callback);

/**
 * @brief 注销命令
 * @param core Shell Core 句柄
 * @param name 命令名称
 * @return 0 成功，-1 失败
 */
int ShellCore_UnregisterCommand(ShellCoreHandle_t core, const char *name);

/**
 * @brief 执行一行命令
 * @param core Shell Core 句柄
 * @param line 命令行字符串
 * @return 命令返回值
 * @note 此函数会解析命令行并调用相应的回调函数
 */
int ShellCore_ExecuteLine(ShellCoreHandle_t core, const char *line);

/**
 * @brief 获取提示符
 * @param core Shell Core 句柄
 * @return 提示符字符串
 */
const char *ShellCore_GetPrompt(ShellCoreHandle_t core);

/**
 * @brief 遍历所有注册的命令
 * @param core Shell Core 句柄
 * @param visitor 访问者函数
 * @param arg 用户参数
 * @note visitor 返回 false 时停止遍历
 */
typedef bool (*ShellCoreCommandVisitor_t)(const char *name, const char *help, void *arg);
void ShellCore_ForEachCommand(ShellCoreHandle_t core, ShellCoreCommandVisitor_t visitor, void *arg);

#endif // MYRTOS_SHELL_CORE_H
