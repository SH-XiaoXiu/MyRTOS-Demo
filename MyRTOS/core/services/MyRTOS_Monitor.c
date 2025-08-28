//
// Created by XiaoXiu on 8/29/2025.
//

#include "MyRTOS_Monitor.h"
#include "MyRTOS.h"
#include "MyRTOS_Log.h"
#include "MyRTOS_Platform.h"
#include "MyRTOS_Driver_Timer.h"
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
static void prvMonitorTask(void *pv) {
    const int COL_WIDTH = 20;
    const char *const SEPARATOR_LINE =
            "---------------------------------------------------------------------------------------------------------";

#if (MY_RTOS_GENERATE_RUN_TIME_STATS == 1)
    TimerHandle_dev_t stats_timer = MyRTOS_Timer_GetHandle(MY_RTOS_STATS_TIMER_ID);
    uint32_t timer_freq = MyRTOS_Timer_GetFreq(stats_timer);
#endif

    while (1) {
        TaskHandle_t taskHandle = NULL;
        TaskStats_t taskStats;
        HeapStats_t heapStats;
        char temp_buffer[128];

        g_monitor_buffer[0] = '\0';
        size_t buffer_capacity = sizeof(g_monitor_buffer);
        size_t current_len = 0;

        snprintf(temp_buffer, sizeof(temp_buffer),
                 "\r\n\r\n----- MyRTOS Monitor (Uptime: %llu ms) -----\r\n", MyRTOS_GetTick());
        strncat(g_monitor_buffer, temp_buffer, buffer_capacity - current_len - 1);
        current_len = strlen(g_monitor_buffer);

        snprintf(temp_buffer, sizeof(temp_buffer),
                 "%-*s%-*s%-*s%-*s%-*s%s\r\n",
                 COL_WIDTH, "Task Name", 8, "ID", 12, "State",
                 12, "Prio(B/C)", 24, "Stack(Used/Total)", "Runtime (us)");
        strncat(g_monitor_buffer, temp_buffer, buffer_capacity - current_len - 1);
        current_len = strlen(g_monitor_buffer);

        snprintf(temp_buffer, sizeof(temp_buffer), "%.*s\r\n", (int) (COL_WIDTH + 8 + 12 + 12 + 24 + 16),
                 SEPARATOR_LINE);
        strncat(g_monitor_buffer, temp_buffer, buffer_capacity - current_len - 1);
        current_len = strlen(g_monitor_buffer);

        taskHandle = Task_GetNextTaskHandle(NULL);
        while (taskHandle != NULL && (buffer_capacity - current_len) > 256) {
            Task_GetInfo(taskHandle, &taskStats);
            char temp_prio[10], temp_stack[32];
            snprintf(temp_prio, sizeof(temp_prio), "%u/%u", taskStats.basePriority, taskStats.currentPriority);
            snprintf(temp_stack, sizeof(temp_stack), "%lu/%lu", taskStats.stackHighWaterMark, taskStats.stackSize);

            uint64_t runtime_us = 0;
#if (MY_RTOS_GENERATE_RUN_TIME_STATS == 1)
            if (timer_freq > 0) {
                runtime_us = (taskStats.runTimeCounter * 1000000) / timer_freq;
            }
#endif

            snprintf(temp_buffer, sizeof(temp_buffer),
                     "%-*.*s%-*d%-*s%-*s%-*s%llu\r\n",
                     COL_WIDTH, (int) MY_RTOS_TASK_NAME_MAX_LEN - 1, taskStats.taskName,
                     8, taskStats.taskId,
                     12, taskStateToStr_Monitor[taskStats.state],
                     12, temp_prio,
                     24, temp_stack,
                     runtime_us);
            strncat(g_monitor_buffer, temp_buffer, buffer_capacity - current_len - 1);
            current_len = strlen(g_monitor_buffer);

            taskHandle = Task_GetNextTaskHandle(taskHandle);
        }

        Heap_GetStats(&heapStats);
        snprintf(temp_buffer, sizeof(temp_buffer), "%.*s\r\n", (int) (COL_WIDTH + 8 + 12 + 12 + 24 + 16),
                 SEPARATOR_LINE);
        strncat(g_monitor_buffer, temp_buffer, buffer_capacity - current_len - 1);
        current_len = strlen(g_monitor_buffer);

        snprintf(temp_buffer, sizeof(temp_buffer),
                 "Heap Info -> Total: %-5u | Free: %-5u | Min Ever Free: %-5u\r\n",
                 (uint32_t) heapStats.totalHeapSize, (uint32_t) heapStats.freeBytesRemaining,
                 (uint32_t) heapStats.minimumEverFreeBytesRemaining);
        strncat(g_monitor_buffer, temp_buffer, buffer_capacity - current_len - 1);
        current_len = strlen(g_monitor_buffer);

        snprintf(temp_buffer, sizeof(temp_buffer),
                 "============================================= End of Report =============================================\r\n");
        strncat(g_monitor_buffer, temp_buffer, buffer_capacity - current_len - 1);
        current_len = strlen(g_monitor_buffer);

        for (size_t i = 0; i < current_len; i++) {
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

    MyRTOS_Log_SetMonitorMode(1);
    Task_Delay(MS_TO_TICKS(50));
    g_monitor_task_handle = Task_Create(prvMonitorTask,
                                        "Monitor",
                                        MY_RTOS_MONITOR_TASK_STACK_SIZE,
                                        NULL,
                                        MY_RTOS_MONITOR_TASK_PRIORITY);
    if (g_monitor_task_handle == NULL) {
        MyRTOS_Log_SetMonitorMode(0);
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
    MyRTOS_Log_SetMonitorMode(0);
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
