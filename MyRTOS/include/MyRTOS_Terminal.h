#ifndef MYRTOS_TERMINAL_H
#define MYRTOS_TERMINAL_H

#include "MyRTOS_Config.h"

#if (MY_RTOS_USE_TERMINAL == 1)

/**
 * @brief 命令处理函数的函数指针类型。
 * @param argc 参数数量
 * @param argv 参数字符串数组
 */
typedef void (*TerminalCommandFunc_t)(int argc, char **argv);

/**
 * @brief 初始化 Terminal 服务所需的基础设施。
 *        此函数本身不启动 Terminal 任务。
 *        应该在 MyRTOS_SystemInit 中被调用。
 */
void MyRTOS_Terminal_Init(void);

/**
 * @brief 启动 Terminal 服务。
 *        将创建 Terminal 任务并准备接收输入。
 * @return 0 成功, -1 失败。
 */
int MyRTOS_Terminal_Start(void);

/**
 * @brief 停止 Terminal 服务。
 *        将销毁 Terminal 任务。
 * @return 0 成功, -1 失败。
 */
int MyRTOS_Terminal_Stop(void);

/**
 * @brief 查询 Terminal 是否正在运行。
 * @return 1 正在运行, 0 已停止。
 */
int MyRTOS_Terminal_IsRunning(void);

/**
 * @brief 向终端服务注册一个新命令。
 *        此函数是线程安全的。
 * @param name 命令的名称 (e.g., "ps")
 * @param help 命令的帮助说明
 * @param func 命令的处理函数
 * @return 0 成功, -1 失败 (如内存不足或命令已存在)
 */
int MyRTOS_Terminal_RegisterCommand(const char* name, const char* help, TerminalCommandFunc_t func);

#else
// 如果禁用, 提供空宏
#define MyRTOS_Terminal_Init()
#define MyRTOS_Terminal_Start() (0)
#define MyRTOS_Terminal_Stop() (0)
#define MyRTOS_Terminal_IsRunning() (0)
#define MyRTOS_Terminal_RegisterCommand(name, help, func) (0)
#endif // MY_RTOS_USE_TERMINAL

#endif // MYRTOS_TERMINAL_H