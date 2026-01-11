/**
 * @file  shell_sysinfo.c
 * @brief 系统信息命令（top, cat）
 */
#include "include/shell.h"

#if MYRTOS_SERVICE_MONITOR_ENABLE == 1

#include "MyRTOS_Monitor.h"
#include "MyRTOS.h"
#include "MyRTOS_IO.h"
#include <stdio.h>
#include <string.h>

#if MYRTOS_SERVICE_VTS_ENABLE == 1
#include "MyRTOS_VTS.h"
#endif

// top命令：实时系统监控
static int cmd_top(shell_handle_t shell, int argc, char *argv[]) {
    (void)shell;
    (void)argc;
    (void)argv;

    MyRTOS_printf("MyRTOS System Monitor - Press Ctrl+C to exit\n\n");

    // 设置为VTS信号接收者（接收Ctrl+C）
#if MYRTOS_SERVICE_VTS_ENABLE == 1
    VTS_SetSignalReceiver(Task_GetCurrentTaskHandle());
#endif

    while (1) {
        // 清屏（简单的方式：打印多行换行）
        MyRTOS_printf("\n========================================\n");

        // 显示堆内存统计
        HeapStats_t heap_stats;
        Monitor_GetHeapStats(&heap_stats);
        MyRTOS_printf("Heap: Total=%u, Free=%u, MinFree=%u\n",
                      (unsigned)heap_stats.total_heap_size,
                      (unsigned)heap_stats.free_bytes_remaining,
                      (unsigned)heap_stats.minimum_ever_free_bytes);

        MyRTOS_printf("----------------------------------------\n");
        MyRTOS_printf("%-12s %-8s %-5s %-9s %-6s\n",
                      "NAME", "STATE", "PRIO", "STACK", "CPU%");
        MyRTOS_printf("----------------------------------------\n");

        // 遍历所有任务
        TaskHandle_t task_h = NULL;
        while ((task_h = Monitor_GetNextTask(task_h)) != NULL) {
            TaskStats_t stats;
            if (Monitor_GetTaskInfo(task_h, &stats) == 0) {
                const char *state_str;
                switch (stats.state) {
                    case TASK_STATE_UNUSED:   state_str = "Unused";   break;
                    case TASK_STATE_READY:    state_str = "Ready";    break;
                    case TASK_STATE_DELAYED:  state_str = "Delayed";  break;
                    case TASK_STATE_BLOCKED:  state_str = "Blocked";  break;
                    case TASK_STATE_SUSPENDED:state_str = "Suspend";  break;
                    default:                  state_str = "Unknown";  break;
                }

                uint32_t stack_used = stats.stack_size_bytes - stats.stack_high_water_mark_bytes;
                uint32_t cpu_percent = stats.cpu_usage_permille / 10;  // 千分比转百分比

                MyRTOS_printf("%-12s %-8s %-5d %4u/%-4u %3u%%\n",
                              stats.task_name,
                              state_str,
                              stats.current_priority,
                              stack_used,
                              stats.stack_size_bytes,
                              cpu_percent);
            }
        }

        MyRTOS_printf("========================================\n");

        // 等待1秒或收到中断信号
#if MYRTOS_SERVICE_VTS_ENABLE == 1
        uint32_t signals = Task_WaitSignal(SIG_INTERRUPT, MS_TO_TICKS(1000),
                                          SIGNAL_WAIT_ANY | SIGNAL_CLEAR_ON_EXIT);
        if (signals & SIG_INTERRUPT) {
            MyRTOS_printf("\nTop interrupted by user.\n");
            break;
        }
#else
        Task_Delay(MS_TO_TICKS(1000));
#endif
    }

    return 0;
}

// cat命令：查看系统信息
static int cmd_cat(shell_handle_t shell, int argc, char *argv[]) {
    (void)shell;

    if (argc < 2) {
        MyRTOS_printf("Usage: cat <heap|tasks>\n");
        MyRTOS_printf("  heap  - 显示堆内存统计\n");
        MyRTOS_printf("  tasks - 显示任务列表\n");
        return -1;
    }

    const char *target = argv[1];

    if (strcmp(target, "heap") == 0) {
        HeapStats_t heap_stats;
        Monitor_GetHeapStats(&heap_stats);
        MyRTOS_printf("堆内存统计:\n");
        MyRTOS_printf("  总大小:     %u 字节\n", (unsigned)heap_stats.total_heap_size);
        MyRTOS_printf("  剩余:       %u 字节\n", (unsigned)heap_stats.free_bytes_remaining);
        MyRTOS_printf("  历史最小:   %u 字节\n", (unsigned)heap_stats.minimum_ever_free_bytes);
        MyRTOS_printf("  使用率:     %u%%\n",
                      (unsigned)((heap_stats.total_heap_size - heap_stats.free_bytes_remaining) * 100 / heap_stats.total_heap_size));
    } else if (strcmp(target, "tasks") == 0) {
        MyRTOS_printf("任务列表:\n");
        MyRTOS_printf("%-16s %-10s %-6s\n", "NAME", "STATE", "PRIO");
        MyRTOS_printf("------------------------------------------\n");

        TaskHandle_t task_h = NULL;
        while ((task_h = Monitor_GetNextTask(task_h)) != NULL) {
            TaskStats_t stats;
            if (Monitor_GetTaskInfo(task_h, &stats) == 0) {
                const char *state_str;
                switch (stats.state) {
                    case TASK_STATE_UNUSED:   state_str = "Unused";   break;
                    case TASK_STATE_READY:    state_str = "Ready";    break;
                    case TASK_STATE_DELAYED:  state_str = "Delayed";  break;
                    case TASK_STATE_BLOCKED:  state_str = "Blocked";  break;
                    case TASK_STATE_SUSPENDED:state_str = "Suspended";break;
                    default:                  state_str = "Unknown";  break;
                }
                MyRTOS_printf("%-16s %-10s %-6d\n",
                              stats.task_name, state_str, stats.current_priority);
            }
        }
    } else {
        MyRTOS_printf("Error: Unknown target '%s'.\n", target);
        MyRTOS_printf("Available targets: heap, tasks\n");
        return -1;
    }

    return 0;
}

void shell_register_sysinfo_commands(shell_handle_t shell) {
    shell_register_command(shell, "top", "实时系统监控工具", cmd_top);
    shell_register_command(shell, "cat", "查看系统信息 (heap|tasks)", cmd_cat);
}

#else

// Monitor 服务未启用
void shell_register_sysinfo_commands(shell_handle_t shell) {
    (void)shell;
}

#endif // MYRTOS_SERVICE_MONITOR_ENABLE
