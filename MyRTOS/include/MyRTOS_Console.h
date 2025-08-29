//
// Created by XiaoXiu on 8/29/2025.
//

#ifndef MYRTOS_CONSOLE_H
#define MYRTOS_CONSOLE_H

#include "MyRTOS_Config.h"
#include "MyRTOS.h"

#if (MY_RTOS_USE_CONSOLE == 1)

/**
 * @brief 控制台的运行模式定义
 */
typedef enum {
    CONSOLE_MODE_LOG, // 默认模式, 用于显示实时日志
    CONSOLE_MODE_MONITOR, // 监视器模式, 全屏刷新系统状态
    CONSOLE_MODE_TERMINAL // 终端模式, 用于命令行交互
} ConsoleMode_t;


/**
 * @brief 初始化 Console 服务。
 *        此函数将创建 Console 任务和所需的队列。
 *        应该在 MyRTOS_SystemInit 中被调用。
 */
void MyRTOS_Console_Init(void);

/**
 * @brief 统一的、线程安全的格式化输出函数。
 *        所有需要向控制台打印信息的上层服务(Log, Monitor, Terminal)
 *        都应调用此函数, 而不是直接访问底层硬件。
 * @param fmt 格式化字符串
 * @param ... 可变参数
 */
void MyRTOS_Console_Printf(const char *fmt, ...);

/**
 * @brief 设置控制台的当前运行模式。
 *        这将决定哪个服务拥有屏幕焦点和输入权。
 * @param mode 要设置的新模式
 */
void MyRTOS_Console_SetMode(ConsoleMode_t mode);

/**
 * @brief 获取控制台的当前运行模式。
 * @return 当前的 ConsoleMode_t
 */
ConsoleMode_t MyRTOS_Console_GetMode(void);


void MyRTOS_Console_RegisterInputConsumer(QueueHandle_t input_queue);


#else
// 如果禁用了 Console, 则提供空实现以避免编译错误
#define MyRTOS_Console_Init()
#define MyRTOS_Console_Printf(fmt, ...)
#define MyRTOS_Console_SetMode(mode)
#define MyRTOS_Console_GetMode() CONSOLE_MODE_LOG

#endif // MY_RTOS_USE_CONSOLE

#endif // MYRTOS_CONSOLE_H
