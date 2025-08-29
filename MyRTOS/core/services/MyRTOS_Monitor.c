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


static TaskHandle_t g_monitor_task_handle = NULL;

static uint64_t g_last_run_time_counters[MAX_TASKS_FOR_STATS] = {0};
static uint64_t g_last_total_run_time = 0;


static const char *const taskStateToStr_Monitor[] = {
    "Unused",
    "Ready",
    "Delayed",
    "Blocked"
};

// 辅助函数，用于直接打印字符串
static void monitor_puts(const char *str) {
    while (*str) {
        MyRTOS_Platform_PutChar(*str++);
    }
}

/**
 * @brief 监视器核心任务 (最终优化版：移除字符串比较)
 */
static void prvMonitorTask(void *pv) {
    const int COL_WIDTH_NAME = 18;
    const char *const SEPARATOR_LINE =
            "--------------------------------------------------------------------------------------------------------------------";

#if (MY_RTOS_GENERATE_RUN_TIME_STATS == 1)
    TimerHandle_dev_t stats_timer = MyRTOS_Timer_GetHandle(MY_RTOS_STATS_TIMER_ID);
    uint32_t timer_freq = MyRTOS_Timer_GetFreq(stats_timer);
#endif

    // 在任务开始时重置一次统计基线
    Task_Delay(MS_TO_TICKS(10));
    MyRTOS_Port_ENTER_CRITICAL(); {
        TaskHandle_t temp_handle = Task_GetNextTaskHandle(NULL);
        while (temp_handle != NULL) {
            TaskStats_t temp_stats;
            Task_GetInfo(temp_handle, &temp_stats);
            if (temp_stats.taskId < MAX_TASKS_FOR_STATS) {
                g_last_run_time_counters[temp_stats.taskId] = temp_stats.runTimeCounter;
            }
            temp_handle = Task_GetNextTaskHandle(temp_handle);
        }
        g_last_total_run_time = MyRTOS_Timer_GetCount(stats_timer);
    }
    MyRTOS_Port_EXIT_CRITICAL();


    while (1) {
        TaskHandle_t taskHandle = NULL;
        HeapStats_t heapStats;
        char temp_buffer[256]; // 行缓冲区

#if (MY_RTOS_GENERATE_RUN_TIME_STATS == 1)
        uint64_t current_total_run_time;
        uint64_t current_run_time_counters[MAX_TASKS_FOR_STATS] = {0};
        MyRTOS_Port_ENTER_CRITICAL(); {
            current_total_run_time = MyRTOS_Timer_GetCount(stats_timer);
            TaskHandle_t temp_handle = Task_GetNextTaskHandle(NULL);
            while (temp_handle != NULL) {
                TaskStats_t temp_stats;
                Task_GetInfo(temp_handle, &temp_stats);
                if (temp_stats.taskId < MAX_TASKS_FOR_STATS) {
                    current_run_time_counters[temp_stats.taskId] = temp_stats.runTimeCounter;
                }
                temp_handle = Task_GetNextTaskHandle(temp_handle);
            }
        }
        MyRTOS_Port_EXIT_CRITICAL();

        uint64_t total_time_delta = current_total_run_time - g_last_total_run_time;

        uint64_t all_app_tasks_runtime_delta = 0;
        TaskHandle_t temp_handle_for_sum = Task_GetNextTaskHandle(NULL);
        while (temp_handle_for_sum != NULL) {
            TaskStats_t temp_stats_for_sum;
            Task_GetInfo(temp_handle_for_sum, &temp_stats_for_sum);

            if ((temp_handle_for_sum != idleTask) &&
                (temp_handle_for_sum != g_monitor_task_handle)) {
                if (temp_stats_for_sum.taskId < MAX_TASKS_FOR_STATS) {
                    all_app_tasks_runtime_delta += (
                        current_run_time_counters[temp_stats_for_sum.taskId] - g_last_run_time_counters[
                            temp_stats_for_sum.taskId]);
                }
            }
            temp_handle_for_sum = Task_GetNextTaskHandle(temp_handle_for_sum);
        }
#endif

        // --- 开始逐行打印 ---
        snprintf(temp_buffer, sizeof(temp_buffer),
                 "\r\n\r\n----- MyRTOS Monitor (Uptime: %llu ms) -----\r\n", MyRTOS_GetTick());
        monitor_puts(temp_buffer);

        snprintf(temp_buffer, sizeof(temp_buffer),
                 "%-*s%-*s%-*s%-*s%-*s%-*s%s\r\n",
                 COL_WIDTH_NAME, "Task Name", 8, "ID", 12, "State",
                 12, "Prio(B/C)", 24, "Stack(Used/Total)", 10, "CPU %", "Runtime (us)");
        monitor_puts(temp_buffer);

        snprintf(temp_buffer, sizeof(temp_buffer), "%.*s\r\n", (int) strlen(SEPARATOR_LINE), SEPARATOR_LINE);
        monitor_puts(temp_buffer);

        taskHandle = Task_GetNextTaskHandle(NULL);
        while (taskHandle != NULL) {
            TaskStats_t taskStats;
            Task_GetInfo(taskHandle, &taskStats);
            char temp_prio[10], temp_stack[32];
            snprintf(temp_prio, sizeof(temp_prio), "%u/%u", taskStats.basePriority, taskStats.currentPriority);
            snprintf(temp_stack, sizeof(temp_stack), "%d/%d", taskStats.stackHighWaterMark, taskStats.stackSize);

            uint64_t runtime_us = 0;
            uint32_t cpu_usage_percent = 0;

#if (MY_RTOS_GENERATE_RUN_TIME_STATS == 1)
            uint64_t current_runtime = current_run_time_counters[taskStats.taskId];
            if (timer_freq > 0) {
                runtime_us = (current_runtime * 1000000) / timer_freq;
            }

            if (taskStats.taskId < MAX_TASKS_FOR_STATS) {
                uint64_t run_time_delta = current_runtime - g_last_run_time_counters[taskStats.taskId];

                if (taskHandle == g_monitor_task_handle || taskHandle == idleTask) {
                    if (total_time_delta > 0) {
                        cpu_usage_percent = (uint32_t) ((run_time_delta * 100) / total_time_delta);
                    }
                } else {
                    if (all_app_tasks_runtime_delta > 0) {
                        cpu_usage_percent = (uint32_t) ((run_time_delta * 100) / all_app_tasks_runtime_delta);
                    } else {
                        cpu_usage_percent = 0;
                    }
                }
            }
#endif
            snprintf(temp_buffer, sizeof(temp_buffer),
                     "%-*.*s%-*d%-*s%-*s%-*s%-*u%llu\r\n",
                     COL_WIDTH_NAME, (int) MY_RTOS_TASK_NAME_MAX_LEN - 1, taskStats.taskName,
                     8, taskStats.taskId,
                     12, taskStateToStr_Monitor[taskStats.state],
                     12, temp_prio,
                     24, temp_stack,
                     10, cpu_usage_percent,
                     runtime_us);
            monitor_puts(temp_buffer);

            taskHandle = Task_GetNextTaskHandle(taskHandle);
        }

        snprintf(temp_buffer, sizeof(temp_buffer), "%.*s\r\n", (int) strlen(SEPARATOR_LINE), SEPARATOR_LINE);
        monitor_puts(temp_buffer);

        Heap_GetStats(&heapStats);
        snprintf(temp_buffer, sizeof(temp_buffer),
                 "Heap Info -> Total: %-5u | Free: %-5u | Min Ever Free: %-5u\r\n",
                 (uint32_t) heapStats.totalHeapSize, (uint32_t) heapStats.freeBytesRemaining,
                 (uint32_t) heapStats.minimumEverFreeBytesRemaining);
        monitor_puts(temp_buffer);

#if (MY_RTOS_GENERATE_RUN_TIME_STATS == 1)
        uint64_t idle_run_time_delta = 0;
        TaskStats_t idle_stats;
        Task_GetInfo(idleTask, &idle_stats);

        if (idle_stats.taskId < MAX_TASKS_FOR_STATS) {
            idle_run_time_delta = current_run_time_counters[idle_stats.taskId] - g_last_run_time_counters[idle_stats.
                                      taskId];
        }

        uint32_t idle_percent = 0;
        if (total_time_delta > 0) {
            idle_percent = (uint32_t) ((idle_run_time_delta * 100) / total_time_delta);
        }

        if (idle_percent > 100) idle_percent = 100;
        uint32_t total_busy_percent = 100 - idle_percent;

        snprintf(temp_buffer, sizeof(temp_buffer), "CPU Usage -> Total Busy: %u%% | System Idle: %u%%\r\n",
                 total_busy_percent, idle_percent);
        monitor_puts(temp_buffer);
#endif

        snprintf(temp_buffer, sizeof(temp_buffer),
                 "================================================ End of Report ==================================================\r\n");
        monitor_puts(temp_buffer);

        // --- 更新下一次计算的基线值 (使用本次快照的数据) ---
        MyRTOS_Port_ENTER_CRITICAL(); {
            g_last_total_run_time = current_total_run_time;
            taskHandle = Task_GetNextTaskHandle(NULL);
            while (taskHandle != NULL) {
                TaskStats_t current_stats;
                Task_GetInfo(taskHandle, &current_stats);
                if (current_stats.taskId < MAX_TASKS_FOR_STATS) {
                    g_last_run_time_counters[current_stats.taskId] = current_run_time_counters[current_stats.taskId];
                }
                taskHandle = Task_GetNextTaskHandle(taskHandle);
            }
        }
        MyRTOS_Port_EXIT_CRITICAL();

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
