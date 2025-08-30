//
// Created by XiaoXiu on 9/6/2025.
// Platform-specific implementation of the standard Monitor component.
// This version faithfully ports the original calculation and printing logic
// and strictly uses public APIs for accessing task information.
// Platform: GD32F4xx
//

#include "MyRTOS.h"

#if (MY_RTOS_USE_MONITOR == 1)
#if (MY_RTOS_USE_SHELL == 1)
#include "MyRTOS_Shell.h"
#endif
#include <stdio.h>
#include "MyRTOS_Std.h"
#include "MyRTOS_Driver_Timer.h"
#include <string.h>

static TaskHandle_t g_monitor_task_handle = NULL;

// 用于增量CPU使用率计算的静态变量
static uint64_t g_last_run_time_counters[MAX_TASKS_FOR_STATS] = {0};
static uint64_t g_last_total_run_time = 0;


static void prvMonitorTask(void *pv);
#if (MY_RTOS_USE_SHELL == 1)
static void cmd_monitor(int argc, char **argv);
#endif

void MyRTOS_Monitor_Init(void) {
#if (MY_RTOS_USE_SHELL == 1)
    MyRTOS_Shell_RegisterCommand("mon", "System monitor. Usage: mon <start|stop>", cmd_monitor);
#endif
}

MyRTOS_Status_t MyRTOS_Monitor_Start(void) {
    if (g_monitor_task_handle != NULL) {
        return MYRTOS_OK;
    }
    g_monitor_task_handle = Task_Create(prvMonitorTask, "Monitor", SYS_MONITOR_TASK_STACK_SIZE, NULL,
                                        SYS_MONITOR_TASK_PRIORITY);
    if (g_monitor_task_handle == NULL) {
        SYS_LOGE("Failed to start Monitor service.");
        return MYRTOS_ERROR;
    }
    SYS_LOGI("Monitor service started.");
    return MYRTOS_OK;
}

MyRTOS_Status_t MyRTOS_Monitor_Stop(void) {
    if (g_monitor_task_handle == NULL) {
        return MYRTOS_OK;
    }
    TaskHandle_t task_to_delete = g_monitor_task_handle;
    g_monitor_task_handle = NULL;
    Task_Delete(task_to_delete);
    printf("\n--- Monitor stopped. ---\n");
    SYS_LOGI("Monitor service stopped.");
    return MYRTOS_OK;
}

int MyRTOS_Monitor_IsRunning(void) {
    if (g_monitor_task_handle != NULL && Task_GetState(g_monitor_task_handle) != TASK_STATE_UNUSED) {
        return 1;
    }
    if (g_monitor_task_handle != NULL) { g_monitor_task_handle = NULL; }
    return 0;
}


/**
 * @brief Monitor后台任务
 */
static void prvMonitorTask(void *pv) {
    const int COL_WIDTH_NAME = 18;
    const char *const SEPARATOR_LINE =
            "--------------------------------------------------------------------------------------------------------------------";
    static const char *const taskStateToStr[] = {"Unused", "Ready", "Delayed", "Blocked"};

#if (MY_RTOS_GENERATE_RUN_TIME_STATS == 1)
    TimerHandle_dev_t stats_timer = MyRTOS_Timer_GetHandle(MY_RTOS_STATS_TIMER_ID);
#endif

    // 在任务开始时重置一次统计基线
    Task_Delay(MS_TO_TICKS(10));
    if (stats_timer) {
        MyRTOS_Port_ENTER_CRITICAL();
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
        MyRTOS_Port_EXIT_CRITICAL();
    }

    while (1) {
        if (g_monitor_task_handle != Task_GetCurrentTaskHandle()) {
            break;
        }

#if (MY_RTOS_GENERATE_RUN_TIME_STATS == 1)
        uint64_t current_total_run_time = 0;
        uint64_t current_run_time_counters[MAX_TASKS_FOR_STATS] = {0};

        if (stats_timer) {
            MyRTOS_Port_ENTER_CRITICAL();
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
            MyRTOS_Port_EXIT_CRITICAL();
        }

        uint64_t total_time_delta = current_total_run_time - g_last_total_run_time;
        uint64_t all_app_tasks_runtime_delta = 0;

        TaskHandle_t temp_handle_for_sum = Task_GetNextTaskHandle(NULL);
        while (temp_handle_for_sum != NULL) {
            TaskStats_t temp_stats_for_sum;
            Task_GetInfo(temp_handle_for_sum, &temp_stats_for_sum);

            if ((temp_handle_for_sum != idleTask) &&
                (temp_handle_for_sum != g_monitor_task_handle) &&
                (temp_stats_for_sum.taskId < MAX_TASKS_FOR_STATS)) {
                all_app_tasks_runtime_delta += (current_run_time_counters[temp_stats_for_sum.taskId] -
                                                g_last_run_time_counters[temp_stats_for_sum.taskId]);
            }
            temp_handle_for_sum = Task_GetNextTaskHandle(temp_handle_for_sum);
        }
#endif

        // --- 开始打印 ---
        printf("\033[2J\033[H"); // 清屏
        printf("----- MyRTOS Monitor (Uptime: %llu ms) -----\n", MyRTOS_GetTick());
        printf("%-*s%-*s%-*s%-*s%-*s%-*s%s\n",
               COL_WIDTH_NAME, "Task Name", 8, "ID", 12, "State",
               12, "Prio(B/C)", 24, "Stack(Used/Total)", 10, "CPU %", "Runtime (us)");
        printf("%s\n", SEPARATOR_LINE);

        TaskHandle_t taskHandle = Task_GetNextTaskHandle(NULL);
        while (taskHandle != NULL) {
            TaskStats_t taskStats;
            Task_GetInfo(taskHandle, &taskStats);

            char temp_prio[10], temp_stack[32];
            snprintf(temp_prio, sizeof(temp_prio), "%u/%u", taskStats.basePriority, taskStats.currentPriority);
            snprintf(temp_stack, sizeof(temp_stack), "%u/%u", (unsigned int) taskStats.stackHighWaterMark,
                     (unsigned int) taskStats.stackSize);

            uint64_t runtime_us = 0;
            uint32_t cpu_usage_percent = 0;

#if (MY_RTOS_GENERATE_RUN_TIME_STATS == 1)
            if (stats_timer) {
                uint32_t timer_freq = MyRTOS_Timer_GetFreq(stats_timer);
                if (timer_freq > 0) {
                    runtime_us = (taskStats.runTimeCounter * 1000000) / timer_freq;
                }
            }

            if (taskStats.taskId < MAX_TASKS_FOR_STATS) {
                uint64_t run_time_delta = current_run_time_counters[taskStats.taskId] - g_last_run_time_counters[
                                              taskStats.taskId];

                if (taskHandle == g_monitor_task_handle || taskHandle == idleTask) {
                    if (total_time_delta > 0) {
                        cpu_usage_percent = (uint32_t) ((run_time_delta * 100) / total_time_delta);
                    }
                } else {
                    if (all_app_tasks_runtime_delta > 0) {
                        cpu_usage_percent = (uint32_t) ((run_time_delta * 100) / all_app_tasks_runtime_delta);
                    }
                }
            }
#endif
            printf("%-*.*s%-*d%-*s%-*s%-*s%-*u%llu\n",
                   COL_WIDTH_NAME, (int)MY_RTOS_TASK_NAME_MAX_LEN - 1, taskStats.taskName,
                   8, taskStats.taskId, 12, taskStateToStr[taskStats.state],
                   12, temp_prio, 24, temp_stack,
                   10, cpu_usage_percent, runtime_us);

            taskHandle = Task_GetNextTaskHandle(taskHandle);
        }

        printf("%s\n", SEPARATOR_LINE);
        HeapStats_t heapStats;
        Heap_GetStats(&heapStats);
        printf("Heap Info -> Total: %-5u | Free: %-5u | Min Ever Free: %-5u\r\n",
               (uint32_t) heapStats.totalHeapSize, (uint32_t) heapStats.freeBytesRemaining,
               (uint32_t) heapStats.minimumEverFreeBytesRemaining);

#if (MY_RTOS_GENERATE_RUN_TIME_STATS == 1)
        uint64_t idle_run_time_delta = 0;

        // ** 关键修正 **
        // 严格通过API获取idleTask的信息
        TaskStats_t idle_stats;
        if (idleTask != NULL) {
            Task_GetInfo(idleTask, &idle_stats);
            if (idle_stats.taskId < MAX_TASKS_FOR_STATS) {
                idle_run_time_delta = current_run_time_counters[idle_stats.taskId] - g_last_run_time_counters[idle_stats
                                          .taskId];
            }
        }

        uint32_t idle_percent = 0;
        if (total_time_delta > 0) {
            idle_percent = (uint32_t) ((idle_run_time_delta * 100) / total_time_delta);
        }
        if (idle_percent > 100) idle_percent = 100;
        uint32_t total_busy_percent = 100 - idle_percent;
        printf("CPU Usage -> Total Busy: %u%% | System Idle: %u%%\n", total_busy_percent, idle_percent);
#endif
        printf(
            "====================================================================================================================\n");

#if (MY_RTOS_GENERATE_RUN_TIME_STATS == 1)
        MyRTOS_Port_ENTER_CRITICAL();
        g_last_total_run_time = current_total_run_time;
        TaskHandle_t temp_handle_update = Task_GetNextTaskHandle(NULL);
        while (temp_handle_update != NULL) {
            TaskStats_t current_stats;
            Task_GetInfo(temp_handle_update, &current_stats);
            if (current_stats.taskId < MAX_TASKS_FOR_STATS) {
                g_last_run_time_counters[current_stats.taskId] = current_run_time_counters[current_stats.taskId];
            }
            temp_handle_update = Task_GetNextTaskHandle(temp_handle_update);
        }
        MyRTOS_Port_EXIT_CRITICAL();
#endif

        Task_Delay(MS_TO_TICKS(SYS_MONITOR_REFRESH_PERIOD_MS));
    }

    g_monitor_task_handle = NULL;
    Task_Delete(NULL);
}


#if (MY_RTOS_USE_SHELL == 1)
static void cmd_monitor(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: mon <start|stop>\n");
        return;
    }
    if (strcmp(argv[1], "start") == 0) {
        MyRTOS_Monitor_Start();
    } else if (strcmp(argv[1], "stop") == 0) {
        MyRTOS_Monitor_Stop();
    } else {
        printf("Invalid argument. Usage: mon <start|stop>\n");
    }
}
#endif

#endif // MY_RTOS_USE_MONITOR
