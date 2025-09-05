//
// Created by XiaoXiu on 9/5/2025.
//

#include "MyRTOS_Service_Config.h"


#if MYRTOS_SERVICE_ASYNC_IO_ENABLE == 1

#include <string.h>
#include "MyRTOS_Utils.h"
#include "MyRTOS_Port.h"
#include "MyRTOS_AsyncIO.h"



/**
 * @brief 后台任务队列传输的消息结构
 */
typedef struct {
    StreamHandle_t target_stream;
    char message[MYRTOS_ASYNCIO_MSG_MAX_SIZE];
} AsyncWriteRequest_t;


static QueueHandle_t g_request_queue = NULL;

static MutexHandle_t g_async_request_lock = NULL;

/**
 * @brief   异步I/O服务的后台工作任务。
 * @details
 *      这是一个消费者任务 它永久阻塞在消息队列上，等待
 *      来自系统其他部分的写入请求 收到请求后，它会代表请求者
 *      执行可能会阻塞的 Stream_Write 操作
 *      通过将I/O阻塞集中到这个低优先级任务
 */
static void AsyncWriter_Task(void *param) {
    (void)param; // 参数未使用
    AsyncWriteRequest_t request;
    for (;;) {
        if (Queue_Receive(g_request_queue, &request, MYRTOS_MAX_DELAY) == 0) {
            continue;
        }
        // 收到请求后 执行实际的写操作。
        if (request.target_stream) {
            Stream_Write(request.target_stream, request.message, strlen(request.message), 0);
        }
    }
}

// -------------------- 公共API实现 --------------------

int AsyncIOService_Init(void) {
    MyRTOS_Port_EnterCritical();
    if (g_request_queue != NULL) {
        MyRTOS_Port_ExitCritical();
        return 0; // 已初始化
    }
    if (g_async_request_lock == NULL) {
        g_async_request_lock = Mutex_Create();
        if (g_async_request_lock == NULL) {
            MyRTOS_Port_ExitCritical();
            return -1;
        }
    }
    MyRTOS_Port_ExitCritical();
    g_request_queue = Queue_Create(MYRTOS_ASYNCIO_QUEUE_LENGTH, sizeof(AsyncWriteRequest_t));
    if (g_request_queue == NULL) {
        g_async_request_lock = NULL;
        return -1; // 队列创建失败
    }
    TaskHandle_t task_h = Task_Create(AsyncWriter_Task, "AsyncIO", MYRTOS_ASYNCIO_TASK_STACK_SIZE, NULL,
                                      MYRTOS_ASYNCIO_TASK_PRIORITY);

    if (task_h == NULL) {
        Queue_Delete(g_request_queue);
        g_request_queue = NULL;
        return -1; //任务创建失败
    }
    return 0;
}

void MyRTOS_AsyncVPrintf(StreamHandle_t stream, const char *format, va_list args) {
    //如果服务未初始化或目标流无效，则静默忽略
    if (g_request_queue == NULL || stream == NULL || g_async_request_lock == NULL) {
        return;
    }
    static AsyncWriteRequest_t request;
    if (MyRTOS_Schedule_IsRunning()) {
        Mutex_Lock(g_async_request_lock);
    }

    request.target_stream = stream;
    // 使用封装的工具格式化字符串
    MyRTOS_FormatV(request.message, sizeof(request.message), format, args);
    Queue_Send(g_request_queue, &request, MS_TO_TICKS(MYRTOS_ASYNCIO_QUEUE_SEND_TIMEOUT));
    if (MyRTOS_Schedule_IsRunning()) {
        Mutex_Unlock(g_async_request_lock);
    }

}

void MyRTOS_AsyncPrintf(StreamHandle_t stream, const char *format, ...) {
    va_list args;
    va_start(args, format);
    MyRTOS_AsyncVPrintf(stream, format, args);
    va_end(args);
}

#endif
