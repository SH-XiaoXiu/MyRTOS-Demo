//
// Created by XiaoXiu on 9/5/2025.
//

/**
 * @file    MyRTOS_AsyncIO.h
 * @brief   MyRTOS 异步 I/O 服务 - 公共接口
 * @details
 *      本服务提供了一个通用的、非阻塞的打印机制。
 *      它通过一个专用的后台任务和消息队列，将耗时的I/O操作
 *      与应用程序任务的执行路径分离，从而保护系统的实时性。
 */
#ifndef MYRTOS_ASYNCIO_H
#define MYRTOS_ASYNCIO_H

#include "MyRTOS_Service_Config.h"

#if MYRTOS_SERVICE_ASYNC_IO_ENABLE == 1

#include "MyRTOS.h"
#include "MyRTOS_IO.h"
#include <stdarg.h>

/**
 * @brief   初始化并启动异步 I/O 服务。
 * @details 此函数必须在调度器启动前，且在使用任何异步打印函数前被调用。
 *          它会创建后台任务和所需的消息队列。
 *          函数内部是线程安全的 做了幂等性设计 可以重复掉
 *
 * @return  int     0 表示成功, -1 表示失败
 */
int AsyncIOService_Init(void);

/**
 * @brief   异步地向指定流打印格式化字符串。
 * @details 这是一个非阻塞函数。它会格式化字符串，将结果打包成一个
 *          写请求，并将其发送到后台任务队列中。
 *          如果队列已满，该打印请求将被静默丢弃，以保证调用任务不被阻塞。
 *
 * @param   stream  目标输出流句柄。
 * @param   format  格式化控制字符串。
 * @param   ...     可变参数。
 */
void MyRTOS_AsyncPrintf(StreamHandle_t stream, const char *format, ...);

/**
 * @brief   MyRTOS_AsyncPrintf 的 va_list 版本。
 * @details 这是实现异步打印的核心 上层模块 (如日志框架) 可以直接
 *          调用此函数，避免自己多次处理 va_list
 *
 * @param   stream  目标输出流句柄。
 * @param   format  格式化控制字符串。
 * @param   args    va_list 类型的参数列表。
 */
void MyRTOS_AsyncVPrintf(StreamHandle_t stream, const char *format, va_list args);

#endif // MYRTOS_SERVICE_ASYNC_IO_ENABLE
#endif // MYRTOS_ASYNCIO_H
