#ifndef MYRTOS_SHELL_H
#define MYRTOS_SHELL_H

#include "MyRTOS.h"
#include "MyRTOS_Service_Config.h"

#if MYRTOS_SERVICE_SHELL_ENABLE == 1
#include "MyRTOS_Stream_Def.h"

// 前置声明Shell实例结构体
struct ShellInstance_t;
typedef struct ShellInstance_t *ShellHandle_t;

/**
 * @brief Shell命令回调函数原型。
 * @param shell_h   执行此命令的Shell实例句柄。
 * @param argc      参数个数 (包括命令本身)
 * @param argv      参数字符串数组。
 * @return int      命令执行的返回值 0 表示成功
 */
typedef int (*ShellCommandCallback_t)(ShellHandle_t shell_h, int argc, char *argv[]);

/**
 * @brief Shell命令定义结构体。
 */
typedef struct {
    const char *name; // 命令名称
    const char *help; // 命令的帮助信息
    ShellCommandCallback_t callback; // 命令处理函数
} ShellCommand_t;

/**
 * @brief Shell命令节定义结构体。
 */
typedef struct ShellCommandNode_t {
    const char *name;
    const char *help;
    ShellCommandCallback_t callback;
    struct ShellCommandNode_t *next; // 指向下一个命令节点的指针
} ShellCommandNode_t;

/**
 * @brief Shell 初始化配置结构体 (用于依赖注入)。
 */
typedef struct {
    const char *prompt; // 命令行提示符 (例如 "> ")
} ShellConfig_t;

/**
 * @brief 创建并初始化一个Shell服务实例。
 * @param config 指向Shell配置的指针。
 * @return ShellHandle_t 成功则返回Shell实例句柄, 失败返回NULL。
 */
ShellHandle_t Shell_Init(const ShellConfig_t *config);

/**
 * @brief 启动Shell服务的任务。
 * @details 此函数会创建一个后台任务来运行Shell的命令接收和处理循环。
 * @param shell_h       Shell_Init 返回的句柄。
 * @param task_name     Shell任务的名称。
 * @param task_priority Shell任务的优先级。
 * @param task_stack_size Shell任务的栈大小。
 * @return TaskHandle_t   成功则返回创建的任务句柄, 失败返回NULL。
 */
TaskHandle_t Shell_Start(ShellHandle_t shell_h, const char *task_name, uint8_t task_priority, uint16_t task_stack_size);

/**
 * @brief 动态注册一个Shell命令。
 * @param shell_h  Shell实例句柄。
 * @param name     命令名称。
 * @param help     命令的帮助信息。
 * @param callback 命令处理函数。
 * @return int 0表示成功，负数表示失败。
 */
int Shell_RegisterCommand(ShellHandle_t shell_h, const char *name, const char *help, ShellCommandCallback_t callback);

/**
 * @brief 动态注销一个Shell命令。
 * @param shell_h Shell实例句柄。
 * @param name    要注销的命令名称。
 * @return int 0表示成功，负数表示未找到。
 */
int Shell_UnregisterCommand(ShellHandle_t shell_h, const char *name);

#endif // MYRTOS_SERVICE_SHELL_ENABLE
#endif // MYRTOS_SHELL_H
