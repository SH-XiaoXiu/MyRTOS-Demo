/**
 * @file  shell_monitor.c
 * @brief 系统监控命令（ps）
 */
#include "shell.h"

#if MYRTOS_SERVICE_MONITOR_ENABLE == 1

#include "MyRTOS_Monitor.h"
#include "MyRTOS_Port.h"
#include "MyRTOS_IO.h"
#include "MyRTOS.h"
#include <stdio.h>

static const char *const g_task_state_str[] = {
    "Unused", "Ready", "Delayed", "Blocked", "Suspended"
};

// ps 命令：显示系统状态
static int cmd_ps(shell_handle_t shell, int argc, char *argv[]) {
    (void)shell;
    (void)argc;
    (void)argv;

    const int COL_WIDTH_NAME = 16;
    TaskStats_t stats[MYRTOS_MAX_CONCURRENT_TASKS];
    int count = 0;

    MyRTOS_Port_EnterCritical();
    TaskHandle_t task = Monitor_GetNextTask(NULL);
    while (task && count < MYRTOS_MAX_CONCURRENT_TASKS) {
        Monitor_GetTaskInfo(task, &stats[count++]);
        task = Monitor_GetNextTask(task);
    }
    MyRTOS_Port_ExitCritical();

    MyRTOS_printf("\nMyRTOS Monitor (Uptime: %llu ms)\n", TICK_TO_MS(MyRTOS_GetTick()));
    MyRTOS_printf("%-*s %-4s %-8s %-10s %-18s %s\n", COL_WIDTH_NAME, "Task Name", "ID", "State",
                  "Prio(B/C)", "Stack (Used/Size)", "Runtime(us)");
    MyRTOS_printf("--\n");

    for (int i = 0; i < count; ++i) {
        TaskStats_t *s = &stats[i];
        char prio_str[12], stack_str[20];
        snprintf(prio_str, sizeof(prio_str), "%u/%u", s->base_priority, s->current_priority);
        snprintf(stack_str, sizeof(stack_str), "%u/%u", (unsigned)s->stack_high_water_mark_bytes,
                 (unsigned)s->stack_size_bytes);
        MyRTOS_printf("%-*s %-4u %-8s %-10s %-18s %llu\n", COL_WIDTH_NAME, s->task_name,
                      (unsigned)Task_GetId(s->task_handle), g_task_state_str[s->state],
                      prio_str, stack_str, s->total_runtime);
    }

    HeapStats_t heap;
    Monitor_GetHeapStats(&heap);
    MyRTOS_printf("Heap Info -> Total: %-5u | Free: %-5u | Min Ever Free: %-5u\n",
                  (unsigned)heap.total_heap_size, (unsigned)heap.free_bytes_remaining,
                  (unsigned)heap.minimum_ever_free_bytes);

    return 0;
}

void shell_register_monitor_commands(shell_handle_t shell) {
    shell_register_command(shell, "ps", "显示系统状态", cmd_ps);
}

#else

// Monitor 服务未启用
void shell_register_monitor_commands(shell_handle_t shell) {
    (void)shell;
}

#endif // MYRTOS_SERVICE_MONITOR_ENABLE
