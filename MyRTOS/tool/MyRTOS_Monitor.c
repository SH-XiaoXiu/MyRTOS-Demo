#include "MyRTOS.h"
#include "MyRTOS_Monitor.h"
#include "MyRTOS_Port.h"
#include "MyRTOS_Log.h"
#include <stdio.h>
#include <string.h>

#if (MY_RTOS_USE_MONITOR == 1)

extern void MyRTOS_Log_SetMonitorMode(int active);

// 内部状态
static TaskHandle_t g_monitor_task_handle = NULL;
static char g_monitor_buffer[MY_RTOS_MONITOR_BUFFER_SIZE];


static const char *const taskStateToStr_Monitor[] = {
    "Unused",
    "Ready",
    "Delayed",
    "Blocked"
};

/**
 * @brief 监视器核心任务
 */
static void prvMonitorTask(void* pv) {
    const int COL_WIDTH = 20;
    const char* const SEPARATOR_LINE = "----------------------------------------------------------------------------------------------------";
    while(1) {
        TaskHandle_t taskHandle = NULL;
        TaskStats_t taskStats;
        HeapStats_t heapStats;
        char temp_buffer[128]; // 一个足够大的行缓冲区

        // --- 每次循环开始时，清空主缓冲区 ---
        g_monitor_buffer[0] = '\0';
        size_t buffer_capacity = sizeof(g_monitor_buffer);
        size_t current_len = 0;

        // --- 使用 snprintf 格式化到行缓冲区，然后用 strncat 安全地追加到主缓冲区 ---

        // 标题
        snprintf(temp_buffer, sizeof(temp_buffer),
                 "\r\n\r\n----- MyRTOS Monitor Snapshot (Uptime: %llu ms) -----\r\n", MyRTOS_GetTick());
        strncat(g_monitor_buffer, temp_buffer, buffer_capacity - current_len - 1);
        current_len = strlen(g_monitor_buffer);

        // 表头
        snprintf(temp_buffer, sizeof(temp_buffer),
                 "%-*s%-*s%-*s%-*s%-*s%s\r\n",
                 COL_WIDTH, "Task Name", COL_WIDTH, "ID", COL_WIDTH, "State",
                 COL_WIDTH, "Prio(B/C)", COL_WIDTH, "Stack(Free/Total)", "Runtime");
        strncat(g_monitor_buffer, temp_buffer, buffer_capacity - current_len - 1);
        current_len = strlen(g_monitor_buffer);

        // 分隔线
        snprintf(temp_buffer, sizeof(temp_buffer), "%.*s\r\n", COL_WIDTH * 5 + 8, SEPARATOR_LINE);
        strncat(g_monitor_buffer, temp_buffer, buffer_capacity - current_len - 1);
        current_len = strlen(g_monitor_buffer);

        // 任务列表
        taskHandle = Task_GetNextTaskHandle(NULL);
        while(taskHandle != NULL && (buffer_capacity - current_len) > 256) {
            Task_GetInfo(taskHandle, &taskStats);
            char temp_prio[10], temp_stack[32];
            snprintf(temp_prio, sizeof(temp_prio), "%u/%u", taskStats.basePriority, taskStats.currentPriority);
            snprintf(temp_stack, sizeof(temp_stack), "%d/%d", taskStats.stackHighWaterMark, taskStats.stackSize);

            snprintf(temp_buffer, sizeof(temp_buffer),
                     "%-*.*s%-*d%-*s%-*s%-*s%llu\r\n",
                     COL_WIDTH, MY_RTOS_TASK_NAME_MAX_LEN - 1, taskStats.taskName,
                     COL_WIDTH, taskStats.taskId,
                     COL_WIDTH, taskStateToStr_Monitor[taskStats.state],
                     COL_WIDTH, temp_prio,
                     COL_WIDTH, temp_stack,
                     taskStats.runTimeCounter);
            strncat(g_monitor_buffer, temp_buffer, buffer_capacity - current_len - 1);
            current_len = strlen(g_monitor_buffer);

            taskHandle = Task_GetNextTaskHandle(taskHandle);
        }

        // 堆信息
        Heap_GetStats(&heapStats);
        snprintf(temp_buffer, sizeof(temp_buffer), "%.*s\r\n", COL_WIDTH * 5 + 8, SEPARATOR_LINE);
        strncat(g_monitor_buffer, temp_buffer, buffer_capacity - current_len - 1);
        current_len = strlen(g_monitor_buffer);

        snprintf(temp_buffer, sizeof(temp_buffer),
                 "Heap Info -> Total: %-5u | Free: %-5u | Min Ever Free: %-5u\r\n",
                 (uint32_t)heapStats.totalHeapSize, (uint32_t)heapStats.freeBytesRemaining, (uint32_t)heapStats.minimumEverFreeBytesRemaining);
        strncat(g_monitor_buffer, temp_buffer, buffer_capacity - current_len - 1);
        current_len = strlen(g_monitor_buffer);

        // 页脚
        snprintf(temp_buffer, sizeof(temp_buffer),
                 "=================================== End of Report =====================================\r\n");
        strncat(g_monitor_buffer, temp_buffer, buffer_capacity - current_len - 1);
        current_len = strlen(g_monitor_buffer);

        // 最终输出
        for(size_t i = 0; i < current_len; i++) {
            MyRTOS_Platform_PutChar(g_monitor_buffer[i]);
        }

        Task_Delay(MS_TO_TICKS(MY_RTOS_MONITOR_TASK_PERIOD_MS));
    }
}


/**
 * @brief 启动系统监视器
 */
int MyRTOS_Monitor_Start(void) {
    if (g_monitor_task_handle != NULL) return -1;

    // 1. 通知日志系统进入“静音模式”
    MyRTOS_Log_SetMonitorMode(1);

    // 2. 短暂延时，确保日志任务处理完最后的消息
    Task_Delay(MS_TO_TICKS(50));

    // 3. 创建监视器任务
    g_monitor_task_handle = Task_Create(prvMonitorTask,
                                        "Monitor",
                                        MY_RTOS_MONITOR_TASK_STACK_SIZE,
                                        NULL,
                                        MY_RTOS_MONITOR_TASK_PRIORITY);

    if (g_monitor_task_handle == NULL) {
        MyRTOS_Log_SetMonitorMode(0); // 创建失败，恢复日志
        PRINT("\r\n[ERROR] Failed to create Monitor task!\r\n");
        return -1;
    }

    return 0;
}

/**
 * @brief 停止系统监视器
 */
int MyRTOS_Monitor_Stop(void) {
    if (g_monitor_task_handle == NULL) return -1;

    Task_Delete(g_monitor_task_handle);
    g_monitor_task_handle = NULL;

    // 恢复日志系统的正常输出
    MyRTOS_Log_SetMonitorMode(0);

    // 打印一条恢复信息
    PRINT("\r\n\r\n----- Monitor Stopped. Normal Log Resumed. -----\r\n\r\n");

    return 0;
}

/**
 * @brief 查询监视器是否正在运行
 */
int MyRTOS_Monitor_IsRunning(void) {
    return (g_monitor_task_handle != NULL);
}

#endif // MY_RTOS_USE_MONITOR
