//
// Created by XiaoXiu on 8/29/2025.
//

#include "MyRTOS.h"
#include "MyRTOS_Console.h"
#include "MyRTOS_Platform.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#if (MY_RTOS_USE_CONSOLE == 1)

// --- 内部状态 ---
static QueueHandle_t g_console_out_queue = NULL;
static TaskHandle_t g_console_task_handle = NULL;
static volatile ConsoleMode_t g_console_mode = CONSOLE_MODE_LOG;
static MutexHandle_t g_direct_print_mutex = NULL; // 用于保护降级打印路径

// --- 内部函数原型 ---
static void prvConsoleTask(void *pv);

void MyRTOS_Console_Init(void) {
    if (g_console_out_queue) return;

    g_console_out_queue = Queue_Create(SYS_CONSOLE_QUEUE_LENGTH, SYS_CONSOLE_MAX_MSG_LENGTH);
    if (!g_console_out_queue) {
        /* Fatal error */
        return;
    }

    g_direct_print_mutex = Mutex_Create();
    if (!g_direct_print_mutex) {
        /* Fatal error */
        return;
    }

    g_console_task_handle = Task_Create(prvConsoleTask, "Console", SYS_CONSOLE_TASK_STACK_SIZE, NULL,
                                        SYS_CONSOLE_TASK_PRIORITY);
    if (!g_console_task_handle) {
        /* Fatal error */
    }
}

/**
 * @brief Console 核心任务 (纯粹的输出服务器)
 */
static void prvConsoleTask(void *pv) {
    char out_buffer[SYS_CONSOLE_MAX_MSG_LENGTH];
    while (1) {
        // 无限期阻塞, 只等待新的打印任务
        if (Queue_Receive(g_console_out_queue, out_buffer, MY_RTOS_MAX_DELAY)) {
            const char *ptr = out_buffer;
            while (*ptr) {
                MyRTOS_Platform_PutChar(*ptr++);
            }
        }
    }
}

void MyRTOS_Console_Printf(const char *fmt, ...) {
    char temp_buffer[SYS_CONSOLE_MAX_MSG_LENGTH];
    va_list args;
    va_start(args, fmt);
    vsnprintf(temp_buffer, sizeof(temp_buffer), fmt, args);
    va_end(args);

    // 如果调度器未运行，直接轮询输出
    if (MyRTOS_Schedule_IsRunning() == 0) {
        const char *ptr = temp_buffer;
        while (*ptr) MyRTOS_Platform_PutChar(*ptr++);
        return;
    }

    // 正常路径：尝试将消息发送到队列。使用0超时，如果队列满则进入降级路径
    if (Queue_Send(g_console_out_queue, temp_buffer, 0) == 1) {
        return; // 发送成功，任务完成
    }

    // 降级路径: 队列已满，强制直接输出（用互斥锁保护，防止多任务同时直接打印造成混乱）
    if (g_direct_print_mutex != NULL) {
        Mutex_Lock(g_direct_print_mutex); {
            const char *prefix = "[DIRECT] "; // 添加前缀以示区分
            const char *ptr = prefix;
            while (*ptr) MyRTOS_Platform_PutChar(*ptr++);

            ptr = temp_buffer;
            while (*ptr) MyRTOS_Platform_PutChar(*ptr++);
        }
        Mutex_Unlock(g_direct_print_mutex);
    }
}

void MyRTOS_Console_SetMode(ConsoleMode_t mode) {
    g_console_mode = mode;
}

ConsoleMode_t MyRTOS_Console_GetMode(void) {
    return g_console_mode;
}


#endif // MY_RTOS_USE_CONSOLE
