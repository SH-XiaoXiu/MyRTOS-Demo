#ifndef MYRTOS_IO_H
#define MYRTOS_IO_H

#include "MyRTOS_Config.h"
#include <stddef.h>

#if (MY_RTOS_USE_STDIO == 1)

// 允许在不同结构体中互相引用指针
struct Stream_t;
struct Task_t;

// --- 流驱动接口定义 ---
// 这是一个 I/O 设备驱动需要实现的函数集合，以适配StdIO系统
typedef struct {
    /**
     * @brief 向流中写入数据。
     * @param stream 指向流对象的指针
     * @param data 要写入的数据
     * @param length 要写入的字节数
     * @return 实际写入的字节数
     */
    int (*write)(struct Stream_t *stream, const void *data, size_t length);

    /**
     * @brief 从流中读取数据。
     * @param stream 指向流对象的指针
     * @param buffer 用于存放读取数据的缓冲区
     * @param length 希望读取的字节数
     * @return 实际读取的字节数
     */
    int (*read)(struct Stream_t *stream, void *buffer, size_t length);

    /**
     * @brief (可选) 设备控制接口，用于特定操作。
     * @param stream 指向流对象的指针
     * @param command 控制命令ID
     * @param arg 命令相关的参数
     * @return 命令执行的结果
     */
    int (*ioctl)(struct Stream_t *stream, int command, void *arg);
} StreamDriver_t;


// --- 流对象结构体 ---
// 一个流的实例，它将驱动程序和具体的硬件实例绑定在一起
typedef struct Stream_t {
    const StreamDriver_t *driver; // 指向实现了功能的驱动
    void *private_data; // 指向设备相关的数据 (例如: 串口句柄, 文件描述符)
} Stream_t;


// --- 全局标准流句柄 ---
// 系统级的默认流，我这里在StdIO初始化时指向默认的控制台
extern Stream_t *g_myrtos_std_in;
extern Stream_t *g_myrtos_std_out;
extern Stream_t *g_myrtos_std_err;

// ==========================================================
//                 应用程序接口 (Application API)
// ==========================================================

/**
 * @brief 初始化标准I/O系统。
 *        创建默认的系统流, 并将 g_myrtos_std* 指向它们。
 */
void MyRTOS_StdIO_Init(void);

/**
 * @brief 格式化输出到指定的流。
 *        这是所有格式化输出函数的基础。
 * @param stream 目标流
 * @param fmt 格式化字符串
 * @param ... 可变参数
 * @return 成功写入的字符数
 */
int MyRTOS_fprintf(Stream_t *stream, const char *fmt, ...);

/**
 * @brief 格式化输出到当前任务的标准输出(stdout)。
 */
#define MyRTOS_printf(...) MyRTOS_fprintf(Task_GetStdOut(NULL), __VA_ARGS__)

/**
 * @brief 从指定的流读取一行(以'\n'结尾)的字符串。
 *        会阻塞直到读取到换行符或缓冲区满。
 * @param buffer 存储字符串的缓冲区
 * @param size 缓冲区的最大大小
 * @param stream 源流
 * @return 成功则返回指向缓冲区的指针, 失败或EOF则返回NULL。
 */
char *MyRTOS_fgets(char *buffer, int size, Stream_t *stream);

/**
 * @brief 从当前任务的标准输入(stdin)读取一行。
 */
#define MyRTOS_gets(buf, size) MyRTOS_fgets(buf, size, Task_GetStdIn(NULL))

#else
#define MyRTOS_printf(...) ((void)0)
#define MyRTOS_fprintf(stream, fmt, ...) (0)
#define MyRTOS_gets(buf, size) (NULL)
#endif // MY_RTOS_USE_STDIO
#endif // MYRTOS_IO_H
