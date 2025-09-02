/**
 * @file  MyRTOS_IO.c
 * @brief MyRTOS IO流服务 - 实现
 */
#include "MyRTOS_IO.h"

#if MYRTOS_SERVICE_IO_ENABLE == 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "MyRTOS_Config.h"
#include "MyRTOS_Extension.h"

/*============================== 内部数据结构 ==============================*/

// 每个任务的标准IO流指针
typedef struct {
    TaskHandle_t task_handle;
    StreamHandle_t std_in;
    StreamHandle_t std_out;
    StreamHandle_t std_err;
} TaskStdIO_t;

// Pipe的私有数据结构
typedef struct {
    QueueHandle_t queue; // Pipe底层使用一个字节队列实现
} PipePrivateData_t;

/*============================== 模块全局变量 ==============================*/

// 用于存储所有任务StdIO信息的数组，大小由内核配置决定
static TaskStdIO_t g_task_stdio_map[MYRTOS_MAX_CONCURRENT_TASKS];
// 默认的系统标准流 (比如可以指向一个UART流)
StreamHandle_t g_system_stdin = NULL;
StreamHandle_t g_system_stdout = NULL;
StreamHandle_t g_system_stderr = NULL;

/*============================== 私有函数原型 ==============================*/

static void stdio_kernel_event_handler(const KernelEventData_t *pEventData);

static TaskStdIO_t *find_task_stdio(TaskHandle_t task_h);

static size_t pipe_read(StreamHandle_t stream, void *buffer, size_t bytes_to_read, uint32_t block_ticks);

static size_t pipe_write(StreamHandle_t stream, const void *buffer, size_t bytes_to_write, uint32_t block_ticks);

/*============================== 内核事件处理器 ==============================*/

// StdIO服务的内核事件处理器，监听任务创建和删除事件，以管理其StdIO流
static void stdio_kernel_event_handler(const KernelEventData_t *pEventData) {
    switch (pEventData->eventType) {
        case KERNEL_EVENT_TASK_CREATE: {
            // 新任务创建时，为其分配一个StdIO槽位
            TaskStdIO_t *new_stdio = find_task_stdio(NULL); // 找一个空槽位
            if (new_stdio) {
                new_stdio->task_handle = pEventData->task;
                TaskHandle_t parent_task = Task_GetCurrentTaskHandle();
                if (parent_task) {
                    // 子任务继承父任务的StdIO流，实现类似shell的管道/重定向功能
                    TaskStdIO_t *parent_stdio = find_task_stdio(parent_task);
                    if (parent_stdio) {
                        new_stdio->std_in = parent_stdio->std_in;
                        new_stdio->std_out = parent_stdio->std_out;
                        new_stdio->std_err = parent_stdio->std_err;
                    }
                } else {
                    // 若无父任务（调度器启动前创建），则继承系统默认流
                    new_stdio->std_in = g_system_stdin;
                    new_stdio->std_out = g_system_stdout;
                    new_stdio->std_err = g_system_stderr;
                }
            }
            break;
        }

        case KERNEL_EVENT_TASK_DELETE: {
            // 任务删除时，释放其StdIO槽位
            TaskStdIO_t *stdio_to_free = find_task_stdio(pEventData->task);
            if (stdio_to_free) {
                // 清理槽位，将句柄置空以备后用
                memset(stdio_to_free, 0, sizeof(TaskStdIO_t));
            }
            break;
        }
        default:
            // 忽略其他事件
            break;
    }
}

/*============================== 辅助函数 ==============================*/

// 查找或分配一个任务的StdIO槽位
static TaskStdIO_t *find_task_stdio(TaskHandle_t task_h) {
    for (int i = 0; i < MYRTOS_MAX_CONCURRENT_TASKS; ++i) {
        if (g_task_stdio_map[i].task_handle == task_h) {
            return &g_task_stdio_map[i];
        }
    }
    return NULL; // 未找到
}

/*============================== 公共API实现 ==============================*/

int StdIOService_Init(void) {
    memset(g_task_stdio_map, 0, sizeof(g_task_stdio_map));
    // 向内核注册事件处理器
    return MyRTOS_RegisterExtension(stdio_kernel_event_handler);
}

StreamHandle_t Task_GetStdIn(TaskHandle_t task_h) {
    if (!task_h)
        task_h = Task_GetCurrentTaskHandle();
    TaskStdIO_t *stdio = find_task_stdio(task_h);
    return stdio ? stdio->std_in : g_system_stdin;
}

StreamHandle_t Task_GetStdOut(TaskHandle_t task_h) {
    if (!task_h)
        task_h = Task_GetCurrentTaskHandle();
    TaskStdIO_t *stdio = find_task_stdio(task_h);
    return stdio ? stdio->std_out : g_system_stdout;
}

StreamHandle_t Task_GetStdErr(TaskHandle_t task_h) {
    if (!task_h)
        task_h = Task_GetCurrentTaskHandle();
    TaskStdIO_t *stdio = find_task_stdio(task_h);
    return stdio ? stdio->std_err : g_system_stderr;
}

void Task_SetStdIn(TaskHandle_t task_h, StreamHandle_t new_stdin) {
    if (!task_h)
        task_h = Task_GetCurrentTaskHandle();
    TaskStdIO_t *stdio = find_task_stdio(task_h);
    if (stdio)
        stdio->std_in = new_stdin;
}

void Task_SetStdOut(TaskHandle_t task_h, StreamHandle_t new_stdout) {
    if (!task_h)
        task_h = Task_GetCurrentTaskHandle();
    TaskStdIO_t *stdio = find_task_stdio(task_h);
    if (stdio)
        stdio->std_out = new_stdout;
}

void Task_SetStdErr(TaskHandle_t task_h, StreamHandle_t new_stderr) {
    if (!task_h)
        task_h = Task_GetCurrentTaskHandle();
    TaskStdIO_t *stdio = find_task_stdio(task_h);
    if (stdio)
        stdio->std_err = new_stderr;
}

/*=========================== 流式 I/O 操作 API 实现 ===========================*/

size_t Stream_Read(StreamHandle_t stream, void *buffer, size_t bytes_to_read, uint32_t block_ticks) {
    // 通过虚函数表调用实际的读函数
    if (stream && stream->p_iface && stream->p_iface->read) {
        return stream->p_iface->read(stream, buffer, bytes_to_read, block_ticks);
    }
    return 0;
}

size_t Stream_Write(StreamHandle_t stream, const void *buffer, size_t bytes_to_write, uint32_t block_ticks) {
    // 通过虚函数表调用实际的写函数
    if (stream && stream->p_iface && stream->p_iface->write) {
        return stream->p_iface->write(stream, buffer, bytes_to_write, block_ticks);
    }
    return 0;
}

int Stream_VPrintf(StreamHandle_t stream, const char *format, va_list args) {
    // 使用配置中定义的缓冲区大小
    char buffer[MYRTOS_IO_PRINTF_BUFFER_SIZE];
    // 安全地格式化字符串到本地缓冲区
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    if (len > 0) {
        // 将格式化后的字符串通过流写出
        Stream_Write(stream, buffer, len, MYRTOS_MAX_DELAY);
    }
    return len;
}

int Stream_Printf(StreamHandle_t stream, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int len = Stream_VPrintf(stream, format, args);
    va_end(args);
    return len;
}

/*======================= Pipe (任务间通信流) API 实现 =======================*/

// Pipe的读实现：从底层队列中读取字节
static size_t pipe_read(StreamHandle_t stream, void *buffer, size_t bytes_to_read, uint32_t block_ticks) {
    PipePrivateData_t *pipe_data = (PipePrivateData_t *) stream->p_private_data;
    uint8_t *p_buf = (uint8_t *) buffer;
    size_t bytes_read = 0;
    // 循环读取，直到满足要求或超时
    while (bytes_read < bytes_to_read) {
        if (Queue_Receive(pipe_data->queue, p_buf + bytes_read, block_ticks) == 1) {
            bytes_read++;
        } else {
            break; // 超时或队列删除
        }
    }
    return bytes_read;
}

// Pipe的写实现：向底层队列中写入字节
static size_t pipe_write(StreamHandle_t stream, const void *buffer, size_t bytes_to_write, uint32_t block_ticks) {
    PipePrivateData_t *pipe_data = (PipePrivateData_t *) stream->p_private_data;
    const uint8_t *p_buf = (const uint8_t *) buffer;
    size_t bytes_written = 0;
    // 循环写入，直到全部写入或超时
    while (bytes_written < bytes_to_write) {
        if (Queue_Send(pipe_data->queue, p_buf + bytes_written, block_ticks) == 1) {
            bytes_written++;
        } else {
            break; // 超时或队列满
        }
    }
    return bytes_written;
}

// 定义Pipe流的虚函数表
static const StreamInterface_t g_pipe_stream_interface = {
        .read = pipe_read,
        .write = pipe_write,
        .control = NULL, // Pipe不支持control方法
};

StreamHandle_t Pipe_Create(size_t buffer_size) {
    // 分配流基类结构体内存
    StreamHandle_t stream = (StreamHandle_t) MyRTOS_Malloc(sizeof(Stream_t));
    if (!stream)
        return NULL;

    // 分配Pipe私有数据结构内存
    PipePrivateData_t *pipe_data = (PipePrivateData_t *) MyRTOS_Malloc(sizeof(PipePrivateData_t));
    if (!pipe_data) {
        MyRTOS_Free(stream);
        return NULL;
    }

    // 创建底层的字节队列
    pipe_data->queue = Queue_Create(buffer_size, sizeof(uint8_t));
    if (!pipe_data->queue) {
        MyRTOS_Free(pipe_data);
        MyRTOS_Free(stream);
        return NULL;
    }

    // 组装流对象：关联接口表和私有数据
    stream->p_iface = &g_pipe_stream_interface;
    stream->p_private_data = pipe_data;

    return stream;
}

void Pipe_Delete(StreamHandle_t pipe_stream) {
    if (pipe_stream && pipe_stream->p_private_data) {
        PipePrivateData_t *pipe_data = (PipePrivateData_t *) pipe_stream->p_private_data;
        // 释放所有相关资源
        Queue_Delete(pipe_data->queue);
        MyRTOS_Free(pipe_data);
        MyRTOS_Free(pipe_stream);
    }
}

#endif // MYRTOS_IO_ENABLE
