/**
 * @file  MyRTOS_IO.c
 * @brief MyRTOS IO������ - ʵ��
 */
#include "MyRTOS_IO.h"

#if MYRTOS_SERVICE_IO_ENABLE == 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "MyRTOS_Config.h"
#include "MyRTOS_Extension.h"

/*============================== �ڲ����ݽṹ ==============================*/

// ÿ������ı�׼IO��ָ��
typedef struct {
    TaskHandle_t task_handle;
    StreamHandle_t std_in;
    StreamHandle_t std_out;
    StreamHandle_t std_err;
} TaskStdIO_t;

// Pipe��˽�����ݽṹ
typedef struct {
    QueueHandle_t queue; // Pipe�ײ�ʹ��һ���ֽڶ���ʵ��
} PipePrivateData_t;

/*============================== ģ��ȫ�ֱ��� ==============================*/

// ���ڴ洢��������StdIO��Ϣ�����飬��С���ں����þ���
static TaskStdIO_t g_task_stdio_map[MYRTOS_MAX_CONCURRENT_TASKS];
// Ĭ�ϵ�ϵͳ��׼�� (�������ָ��һ��UART��)
StreamHandle_t g_system_stdin = NULL;
StreamHandle_t g_system_stdout = NULL;
StreamHandle_t g_system_stderr = NULL;

/*============================== ˽�к���ԭ�� ==============================*/

static void stdio_kernel_event_handler(const KernelEventData_t *pEventData);

static TaskStdIO_t *find_task_stdio(TaskHandle_t task_h);

static size_t pipe_read(StreamHandle_t stream, void *buffer, size_t bytes_to_read, uint32_t block_ticks);

static size_t pipe_write(StreamHandle_t stream, const void *buffer, size_t bytes_to_write, uint32_t block_ticks);

/*============================== �ں��¼������� ==============================*/

// StdIO������ں��¼����������������񴴽���ɾ���¼����Թ�����StdIO��
static void stdio_kernel_event_handler(const KernelEventData_t *pEventData) {
    switch (pEventData->eventType) {
        case KERNEL_EVENT_TASK_CREATE: {
            // �����񴴽�ʱ��Ϊ�����һ��StdIO��λ
            TaskStdIO_t *new_stdio = find_task_stdio(NULL); // ��һ���ղ�λ
            if (new_stdio) {
                new_stdio->task_handle = pEventData->task;
                TaskHandle_t parent_task = Task_GetCurrentTaskHandle();
                if (parent_task) {
                    // ������̳и������StdIO����ʵ������shell�Ĺܵ�/�ض�����
                    TaskStdIO_t *parent_stdio = find_task_stdio(parent_task);
                    if (parent_stdio) {
                        new_stdio->std_in = parent_stdio->std_in;
                        new_stdio->std_out = parent_stdio->std_out;
                        new_stdio->std_err = parent_stdio->std_err;
                    }
                } else {
                    // ���޸����񣨵���������ǰ����������̳�ϵͳĬ����
                    new_stdio->std_in = g_system_stdin;
                    new_stdio->std_out = g_system_stdout;
                    new_stdio->std_err = g_system_stderr;
                }
            }
            break;
        }

        case KERNEL_EVENT_TASK_DELETE: {
            // ����ɾ��ʱ���ͷ���StdIO��λ
            TaskStdIO_t *stdio_to_free = find_task_stdio(pEventData->task);
            if (stdio_to_free) {
                // �����λ��������ÿ��Ա�����
                memset(stdio_to_free, 0, sizeof(TaskStdIO_t));
            }
            break;
        }
        default:
            // ���������¼�
            break;
    }
}

/*============================== �������� ==============================*/

// ���һ����һ�������StdIO��λ
static TaskStdIO_t *find_task_stdio(TaskHandle_t task_h) {
    for (int i = 0; i < MYRTOS_MAX_CONCURRENT_TASKS; ++i) {
        if (g_task_stdio_map[i].task_handle == task_h) {
            return &g_task_stdio_map[i];
        }
    }
    return NULL; // δ�ҵ�
}

/*============================== ����APIʵ�� ==============================*/

int StdIOService_Init(void) {
    memset(g_task_stdio_map, 0, sizeof(g_task_stdio_map));
    // ���ں�ע���¼�������
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

/*=========================== ��ʽ I/O ���� API ʵ�� ===========================*/

size_t Stream_Read(StreamHandle_t stream, void *buffer, size_t bytes_to_read, uint32_t block_ticks) {
    // ͨ���麯�������ʵ�ʵĶ�����
    if (stream && stream->p_iface && stream->p_iface->read) {
        return stream->p_iface->read(stream, buffer, bytes_to_read, block_ticks);
    }
    return 0;
}

size_t Stream_Write(StreamHandle_t stream, const void *buffer, size_t bytes_to_write, uint32_t block_ticks) {
    // ͨ���麯�������ʵ�ʵ�д����
    if (stream && stream->p_iface && stream->p_iface->write) {
        return stream->p_iface->write(stream, buffer, bytes_to_write, block_ticks);
    }
    return 0;
}

int Stream_VPrintf(StreamHandle_t stream, const char *format, va_list args) {
    // ʹ�������ж���Ļ�������С
    char buffer[MYRTOS_IO_PRINTF_BUFFER_SIZE];
    // ��ȫ�ظ�ʽ���ַ��������ػ�����
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    if (len > 0) {
        // ����ʽ������ַ���ͨ����д��
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

/*======================= Pipe (�����ͨ����) API ʵ�� =======================*/

// Pipe�Ķ�ʵ�֣��ӵײ�����ж�ȡ�ֽ�
static size_t pipe_read(StreamHandle_t stream, void *buffer, size_t bytes_to_read, uint32_t block_ticks) {
    PipePrivateData_t *pipe_data = (PipePrivateData_t *) stream->p_private_data;
    uint8_t *p_buf = (uint8_t *) buffer;
    size_t bytes_read = 0;
    // ѭ����ȡ��ֱ������Ҫ���ʱ
    while (bytes_read < bytes_to_read) {
        if (Queue_Receive(pipe_data->queue, p_buf + bytes_read, block_ticks) == 1) {
            bytes_read++;
        } else {
            break; // ��ʱ�����ɾ��
        }
    }
    return bytes_read;
}

// Pipe��дʵ�֣���ײ������д���ֽ�
static size_t pipe_write(StreamHandle_t stream, const void *buffer, size_t bytes_to_write, uint32_t block_ticks) {
    PipePrivateData_t *pipe_data = (PipePrivateData_t *) stream->p_private_data;
    const uint8_t *p_buf = (const uint8_t *) buffer;
    size_t bytes_written = 0;
    // ѭ��д�룬ֱ��ȫ��д���ʱ
    while (bytes_written < bytes_to_write) {
        if (Queue_Send(pipe_data->queue, p_buf + bytes_written, block_ticks) == 1) {
            bytes_written++;
        } else {
            break; // ��ʱ�������
        }
    }
    return bytes_written;
}

// ����Pipe�����麯����
static const StreamInterface_t g_pipe_stream_interface = {
        .read = pipe_read,
        .write = pipe_write,
        .control = NULL, // Pipe��֧��control����
};

StreamHandle_t Pipe_Create(size_t buffer_size) {
    // ����������ṹ���ڴ�
    StreamHandle_t stream = (StreamHandle_t) MyRTOS_Malloc(sizeof(Stream_t));
    if (!stream)
        return NULL;

    // ����Pipe˽�����ݽṹ�ڴ�
    PipePrivateData_t *pipe_data = (PipePrivateData_t *) MyRTOS_Malloc(sizeof(PipePrivateData_t));
    if (!pipe_data) {
        MyRTOS_Free(stream);
        return NULL;
    }

    // �����ײ���ֽڶ���
    pipe_data->queue = Queue_Create(buffer_size, sizeof(uint8_t));
    if (!pipe_data->queue) {
        MyRTOS_Free(pipe_data);
        MyRTOS_Free(stream);
        return NULL;
    }

    // ��װ�����󣺹����ӿڱ��˽������
    stream->p_iface = &g_pipe_stream_interface;
    stream->p_private_data = pipe_data;

    return stream;
}

void Pipe_Delete(StreamHandle_t pipe_stream) {
    if (pipe_stream && pipe_stream->p_private_data) {
        PipePrivateData_t *pipe_data = (PipePrivateData_t *) pipe_stream->p_private_data;
        // �ͷ����������Դ
        Queue_Delete(pipe_data->queue);
        MyRTOS_Free(pipe_data);
        MyRTOS_Free(pipe_stream);
    }
}

#endif // MYRTOS_IO_ENABLE
