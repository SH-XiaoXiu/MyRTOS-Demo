/**
 * @file  shell.h
 * @brief Shell 程序接口定义
 */
#ifndef SHELL_H
#define SHELL_H

#include <stddef.h>
#include <stdbool.h>

// Shell Core 句柄（不透明类型）
typedef struct shell_core_t *shell_handle_t;

// 命令回调函数类型
typedef int (*shell_command_callback_t)(shell_handle_t shell, int argc, char *argv[]);

// 命令遍历回调
typedef bool (*shell_command_visitor_t)(const char *name, const char *help, void *arg);

/**
 * @brief 创建Shell实例
 * @param prompt 提示符字符串
 * @return Shell句柄，失败返回NULL
 */
shell_handle_t shell_create(const char *prompt);

/**
 * @brief 销毁Shell实例
 * @param shell Shell句柄
 */
void shell_destroy(shell_handle_t shell);

/**
 * @brief 注册命令
 * @param shell Shell句柄
 * @param name 命令名称
 * @param help 帮助信息
 * @param callback 命令回调函数
 * @return 0成功，-1失败，-2命令已存在
 */
int shell_register_command(shell_handle_t shell,
                          const char *name,
                          const char *help,
                          shell_command_callback_t callback);

/**
 * @brief 注销命令
 * @param shell Shell句柄
 * @param name 命令名称
 * @return 0成功，-1失败
 */
int shell_unregister_command(shell_handle_t shell, const char *name);

/**
 * @brief 执行一行命令
 * @param shell Shell句柄
 * @param line 命令行字符串
 * @return 命令返回值
 */
int shell_execute_line(shell_handle_t shell, const char *line);

/**
 * @brief 获取提示符
 * @param shell Shell句柄
 * @return 提示符字符串
 */
const char *shell_get_prompt(shell_handle_t shell);

/**
 * @brief 遍历所有命令
 * @param shell Shell句柄
 * @param visitor 遍历回调函数
 * @param arg 用户参数
 */
void shell_foreach_command(shell_handle_t shell, shell_command_visitor_t visitor, void *arg);

// ============================================
// 命令注册函数（各模块提供）
// ============================================

/**
 * @brief 注册内置命令（help）
 */
void shell_register_builtin_commands(shell_handle_t shell);

/**
 * @brief 注册监控命令（ps）
 */
void shell_register_monitor_commands(shell_handle_t shell);

/**
 * @brief 注册日志命令（logall, log, loglevel）
 */
void shell_register_log_commands(shell_handle_t shell);

/**
 * @brief 注册进程命令（run, jobs, kill, fg, bg, ls）
 */
void shell_register_process_commands(shell_handle_t shell);

/**
 * @brief 注册平台命令（reboot）
 */
void shell_register_platform_commands(shell_handle_t shell);

#endif // SHELL_H
