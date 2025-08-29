//
// Created by XiaoXiu on 8/29/2025.
//

//这是一个标准输入输出规范 用于MyRTOS系统 起草阶段.

#if 0
#ifndef MYRTOS_STDIO_H
#define MYRTOS_STDIO_H

#include <stdarg.h>
#include "MyRTOS.h"

// --- 前置声明 ---
struct Stream_t;

// --- 流驱动接口定义 ---
// 这是实现一个具体 I/O 设备所需提供的函数集合
typedef struct {
    /**
     * @brief 向流中写入数据。
     * @param stream 指向流对象的指针
     * @param data 要写入的数据
     * @param length 要写入的长度
     * @return 实际写入的字节数
     */
    int (*write)(struct Stream_t *stream, const void *data, size_t length);

    /**
     * @brief 从流中读取数据。
     * @param stream 指向流对象的指针
     * @param buffer 用于存放读取数据的缓冲区
     * @param length 要读取的长度
     * @return 实际读取的字节数
     */
    int (*read)(struct Stream_t *stream, void *buffer, size_t length);

    // (可选) 其他控制函数, 如 seek, flush, ioctl 等
    // int (*ioctl)(struct Stream_t *stream, int command, void *arg);
} StreamDriver_t;


// --- 流对象结构体 ---
// 一个流的实例, 包含了它的驱动和私有数据
typedef struct Stream_t {
    const StreamDriver_t *driver; // 指向实现了具体功能的驱动
    void *private_data; // 指向设备相关的数据 (例如: 串口号, 文件句柄)
    // (可选) 可以在此添加流的标志位, 如 读/写/追加, 阻塞/非阻塞 等
} Stream_t;


// --- 全局标准流句柄 ---
// 这三个是系统级的默认流, 通常指向控制台
extern Stream_t *g_myrtos_stdin;
extern Stream_t *g_myrtos_stdout;
extern Stream_t *g_myrtos_stderr;


// ==========================================================
//                 应用程序接口 (Application API)
// ==========================================================

/**
 * @brief 初始化标准I/O系统。
 *        将创建默认的控制台流, 并将 g_myrtos_std* 指向它们。
 */
void MyRTOS_StdIO_Init(void);


/**
 * @brief 格式化输出到指定的流。
 * @param stream 目标流
 * @param fmt 格式化字符串
 */
int MyRTOS_fprintf(Stream_t *stream, const char *fmt, ...);

/**
 * @brief 格式化输出到当前任务的 stdout。
 * @param fmt 格式化字符串
 */
int MyRTOS_printf(const char *fmt, ...);


/**
 * @brief 从指定的流读取数据。
 * @param stream 源流
 * @param buffer 缓冲区
 * @param length 要读取的长度
 * @return 实际读取的字节数
 */
int MyRTOS_fread(Stream_t *stream, void *buffer, size_t length);

/**
 * @brief 从当前任务的 stdin 读取数据。
 * @param buffer 缓冲区
 * @param length 要读取的长度
 * @return 实际读取的字节数
 */
int MyRTOS_read(void *buffer, size_t length);


/**
 * @brief 重定向任务的标准流。
 * @param task_h 目标任务句柄 (NULL 表示当前任务)
 * @param std_stream_id 0 for stdin, 1 for stdout, 2 for stderr
 * @param new_stream 要重定向到的新流
 * @return 0 成功, -1 失败
 */
int MyRTOS_Task_RedirectStdStream(TaskHandle_t task_h, int std_stream_id, Stream_t *new_stream);


#endif // MYRTOS_STDIO_H
#endif
