#ifndef MYRTOS_SHELL_H
#define MYRTOS_SHELL_H

#include "MyRTOS_Config.h"

#if (MY_RTOS_USE_SHELL == 1)

#include "MyRTOS.h" // For TaskHandle_t

/**
 * @brief Shell命令处理函数的函数指针类型。
 * @param argc 参数数量
 * @param argv 参数字符串数组
 */
typedef void (*ShellCommandFunc_t)(int argc, char **argv);

/**
 * @brief 初始化 Shell 服务。
 *        主要是注册内置命令，本身不创建任务。
 *        应在 MyRTOS_SystemInit 中被调用。
 */
void MyRTOS_Shell_Init(void);

/**
 * @brief 启动 Shell 服务。
 *        此函数会创建并启动Shell主任务。如果Shell已在运行，则不做任何事。
 * @return 0 成功, -1 失败 (如任务创建失败)。
 */
int MyRTOS_Shell_Start(void);

/**
 * @brief 停止 Shell 服务。
 *        此函数会销毁正在运行的Shell任务。如果Shell未运行，则不做任何事。
 * @return 0 成功。
 */
int MyRTOS_Shell_Stop(void);

/**
 * @brief 查询 Shell 服务是否正在运行。
 * @return 1 正在运行, 0 已停止。
 */
int MyRTOS_Shell_IsRunning(void);

/**
 * @brief 向 Shell 服务注册一个新命令。
 * @param name 命令的名称 (e.g., "ps")
 * @param help 命令的帮助说明
 * @param func 命令的处理函数
 * @return 0 成功, -1 失败 (如内存不足)
 */
int MyRTOS_Shell_RegisterCommand(const char* name, const char* help, ShellCommandFunc_t func);

#else
#define MyRTOS_Shell_Init()
#define MyRTOS_Shell_Start()      (0)
#define MyRTOS_Shell_Stop()       (0)
#define MyRTOS_Shell_IsRunning()  (0)
#define MyRTOS_Shell_RegisterCommand(name, help, func) (0)
#endif // MY_RTOS_USE_SHELL

#endif // MYRTOS_SHELL_H