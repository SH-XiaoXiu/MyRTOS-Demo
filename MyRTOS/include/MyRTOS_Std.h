#ifndef MYRTOS_STANDARD_H
#define MYRTOS_STANDARD_H

#include "MyRTOS_Config.h"

// ======================================================================
//                       标准便捷宏 (Standard Convenience Macros)
// ======================================================================
// 为了方便应用程序开发，提供一组类似标准C库的宏

#if (MY_RTOS_USE_STDIO == 1)
#define printf(...)    MyRTOS_printf(__VA_ARGS__)
//打印字符串并自动换行
#define puts(s)        MyRTOS_printf("%s\n", s)
//打印单个字符
#define putchar(c)     MyRTOS_printf("%c", c)
#else
#define printf(...)    ((void)0)
#define puts(s)        ((void)0)
#define putchar(c)     ((void)0)
#endif


// ======================================================================
//                       标准服务头文件包含
// ======================================================================
// 这个头文件旨在提供MyRTOS最常用和标准的服务接口。

#if (MY_RTOS_USE_STDIO == 1)
#include "MyRTOS_IO.h"
#endif

#if (MY_RTOS_USE_LOG == 1)
#include "MyRTOS_Log.h"
#endif

// ======================================================================
//                       标准返回码 (Standard Return Codes)
// ======================================================================
// 定义一套统一的返回码，用于所有API
typedef enum {
    MYRTOS_OK = 0, // 操作成功
    MYRTOS_ERROR = -1, // 通用错误
    MYRTOS_TIMEOUT = -2, // 操作超时
    MYRTOS_INVALID_ARG = -3, // 无效参数
    MYRTOS_NO_MEM = -4, // 内存不足
} MyRTOS_Status_t;


// ======================================================================
//                       标准组件接口：系统监视器
// ======================================================================
#if (MY_RTOS_USE_MONITOR == 1)

/**
 * @brief 启动系统监视器服务。
 *        将创建并运行一个后台任务，该任务会周期性地将系统状态
 *        打印到标准输出流。如果已在运行，则无效果。
 * @return MYRTOS_OK 成功, MYRTOS_ERROR 失败。
 */
MyRTOS_Status_t MyRTOS_Monitor_Start(void);

/**
 * @brief 停止系统监视器服务。
 *        将销毁后台的监视器任务。如果未运行，则无效果。
 * @return MYRTOS_OK 成功。
 */
MyRTOS_Status_t MyRTOS_Monitor_Stop(void);

/**
 * @brief 查询监视器服务是否正在运行。
 * @return 1 正在运行, 0 已停止。
 */
int MyRTOS_Monitor_IsRunning(void);

#endif // MY_RTOS_USE_MONITOR


// ======================================================================
//                       其他未来可能的标准组件.
// ======================================================================
/*
#if (MY_RTOS_USE_FILESYSTEM == 1)
    // 文件系统相关的标准接口定义...
#endif
*/


#endif // MY_RTOS_STANDARD_H
