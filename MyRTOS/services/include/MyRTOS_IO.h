/**
 * @file  MyRTOS_IO.h
 * @brief MyRTOS IO流服务 - 公共接口
 * @details 提供任务标准IO重定向、流式读写、管道(Pipe)等功能
 */
#ifndef MYRTOS_IO_H
#define MYRTOS_IO_H

#include "MyRTOS_Service_Config.h"


#ifndef MYRTOS_SERVICE_IO_ENABLE
#define MYRTOS_IO_ENABLE 0
#endif

#if MYRTOS_SERVICE_IO_ENABLE == 1


#include <stdarg.h>
#include "MyRTOS.h"
#include "MyRTOS_Stream_Def.h"


/*================================== 标准流 API ==================================*/

/**
 * @brief 初始化标准IO服务扩展。
 * @details 必须在创建任何任务之前调用，以注册内核事件监听器，
 *          确保任务能够正确继承父任务的标准流。
 * @return int 0 表示成功, -1 表示失败。
 */
int StdIOService_Init(void);

/**
 * @brief 获取指定任务的标准输入流。
 * @param task_h 任务句柄 (若为NULL，则获取当前任务的流)
 * @return StreamHandle_t 指向该任务stdin流的句柄
 */
StreamHandle_t Task_GetStdIn(TaskHandle_t task_h);

/**
 * @brief 获取指定任务的标准输出流。
 * @param task_h 任务句柄 (若为NULL，则获取当前任务的流)
 * @return StreamHandle_t 指向该任务stdout流的句柄
 */
StreamHandle_t Task_GetStdOut(TaskHandle_t task_h);

/**
 * @brief 获取指定任务的标准错误流。
 * @param task_h 任务句柄 (若为NULL，则获取当前任务的流)
 * @return StreamHandle_t 指向该任务stderr流的句柄
 */
StreamHandle_t Task_GetStdErr(TaskHandle_t task_h);

/**
 * @brief 重定向任务的标准输入流。
 * @param task_h 要操作的任务句柄 (NULL 表示当前任务)
 * @param new_stdin 新的标准输入流句柄
 */
void Task_SetStdIn(TaskHandle_t task_h, StreamHandle_t new_stdin);

/**
 * @brief 重定向任务的标准输出流。
 * @param task_h 要操作的任务句柄 (NULL 表示当前任务)
 * @param new_stdout 新的标准输出流句柄
 */
void Task_SetStdOut(TaskHandle_t task_h, StreamHandle_t new_stdout);

/**
 * @brief 重定向任务的标准错误流。
 * @param task_h 要操作的任务句柄 (NULL 表示当前任务)
 * @param new_stderr 新的标准错误流句柄
 */
void Task_SetStdErr(TaskHandle_t task_h, StreamHandle_t new_stderr);

/*================================ 流式 I/O 操作 API ===============================*/

/**
 * @brief 从指定的流中读取数据。
 * @param stream        [in] 流句柄
 * @param buffer        [out] 存放读取数据的缓冲区
 * @param bytes_to_read [in] 希望读取的字节数
 * @param block_ticks   [in] 阻塞等待时间 (ticks)
 * @return size_t 实际读取到的字节数
 */
size_t Stream_Read(StreamHandle_t stream, void *buffer, size_t bytes_to_read, uint32_t block_ticks);

/**
 * @brief 向指定的流中写入数据。
 * @param stream         [in] 流句柄
 * @param buffer         [in] 待写入数据的缓冲区
 * @param bytes_to_write [in] 希望写入的字节数
 * @param block_ticks    [in] 阻塞等待时间 (ticks)
 * @return size_t 实际写入的字节数
 */
size_t Stream_Write(StreamHandle_t stream, const void *buffer, size_t bytes_to_write, uint32_t block_ticks);

/**
 * @brief 向指定的流中写入格式化字符串。
 * @param stream [in] 流句柄
 * @param format [in] 格式化字符串
 * @param ...    [in] 可变参数
 * @return int 成功写入的字符数
 */
int Stream_Printf(StreamHandle_t stream, const char *format, ...);

/**
 * @brief Stream_Printf的va_list版本。
 */
int Stream_VPrintf(StreamHandle_t stream, const char *format, va_list args);


/**
 * @brief 获取一个指向 "null" 流的句柄-单例.
 * @details 是一种特殊的流:
 *          - 读取操作会立即返回0 (模拟文件结束 EOF).
 *          - 写入操作会接受所有数据但直接丢弃 (黑洞流).
 *          可用于重定向不需要的输入或输出.
 * @return StreamHandle_t 指向 null 流的句柄.
 */
StreamHandle_t NullStream_Get(void);

// 默认操作当前任务的标准流的便捷宏
#define MyRTOS_printf(format, ...) Stream_Printf(Task_GetStdOut(NULL), format, ##__VA_ARGS__)
#define MyRTOS_putchar(c) Stream_Write(Task_GetStdOut(NULL), &(c), 1, MYRTOS_MAX_DELAY)
#define MyRTOS_getchar()                                                                                               \
    ({                                                                                                                 \
        char __ch;                                                                                                     \
        Stream_Read(Task_GetStdIn(NULL), &__ch, 1, MYRTOS_MAX_DELAY);                                                  \
        __ch;                                                                                                          \
    })
/*============================== Pipe (任务间通信流) API =============================*/

/**
 * @brief 创建一个管道（Pipe）。
 * @details 管道是一个内核管理的FIFO字节缓冲区，它实现了流接口，
 *          可用于连接一个任务的stdout和另一个任务的stdin，实现任务间通信。
 * @param buffer_size 管道内部缓冲区的大小（字节）。
 * @return StreamHandle_t 成功则返回管道流的句柄，失败则返回 NULL。
 */
StreamHandle_t Pipe_Create(size_t buffer_size);

/**
 * @brief 删除一个管道。
 * @param pipe_stream 要删除的管道流句柄。
 */
void Pipe_Delete(StreamHandle_t pipe_stream);

#endif // MYRTOS_IO_ENABLE

#endif // MYRTOS_IO_H
