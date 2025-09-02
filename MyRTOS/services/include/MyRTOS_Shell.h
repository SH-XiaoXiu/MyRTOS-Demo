/**
 * @file  MyRTOS_Shell.h
 * @brief MyRTOS 命令行(Shell)服务 - 公共接口
 * @details 提供一个可交互的命令行界面，用于执行用户定义的命令。依赖IO流模块。
 */
#ifndef MYRTOS_SHELL_H
#define MYRTOS_SHELL_H

#include "MyRTOS.h"
#include "MyRTOS_Service_Config.h"


#ifndef MYRTOS_SERVICE_SHELL_ENABLE
#define MYRTOS_SERVICE_SHELL_ENABLE 0
#endif

#if MYRTOS_SERVICE_SHELL_ENABLE == 1

#include "MyRTOS_Stream_Def.h"

// 前置声明Shell实例结构体
struct ShellInstance_t;
/** @brief Shell实例句柄类型 */
typedef struct ShellInstance_t *ShellHandle_t;

/**
 * @brief Shell命令回调函数原型。
 * @param shell_h   [in] 执行此命令的Shell实例句柄。
 * @param argc      [in] 参数个数 (包括命令本身)。
 * @param argv      [in] 参数字符串数组。
 * @return int      命令执行的返回值 (通常 0 表示成功)。
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
 * @param config        [in] 指向Shell配置的指针 (包含依赖注入的流)。
 * @param commands      [in] 指向命令表数组的指针。
 * @param command_count [in] 命令表中的命令数量。
 * @return ShellHandle_t    成功则返回Shell实例句柄, 失败返回NULL。
 */
ShellHandle_t Shell_Init(const ShellConfig_t *config);

/**
 * @brief 启动Shell服务的任务。
 * @details 此函数会创建一个后台任务来运行Shell的命令接收和处理循环。
 * @param shell_h       [in] Shell_Init 返回的句柄。
 * @param task_name     [in] Shell任务的名称。
 * @param task_priority [in] Shell任务的优先级。
 * @param task_stack_size [in] Shell任务的栈大小。
 * @return TaskHandle_t   成功则返回创建的任务句柄, 失败返回NULL。
 */
TaskHandle_t Shell_Start(ShellHandle_t shell_h, const char *task_name, uint8_t task_priority, uint16_t task_stack_size);

/**
 * @brief 获取与Shell实例关联的流句柄。
 * @details 这对于命令回调函数需要直接写回终端非常有用。
 * @param shell_h [in] Shell实例句柄。
 * @return StreamHandle_t 流句柄。
 */
StreamHandle_t Shell_GetStream(ShellHandle_t shell_h);


int Shell_RegisterCommand(ShellHandle_t shell_h, const char *name, const char *help, ShellCommandCallback_t callback);

int Shell_UnregisterCommand(ShellHandle_t shell_h, const char *name);

#endif // MYRTOS_SHELL_ENABLE

#endif // MYRTOS_SHELL_H
