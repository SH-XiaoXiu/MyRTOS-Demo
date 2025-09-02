//
// Created by XiaoXiu on 9/1/2025.
//

#include "MyRTOS_Service_Config.h"
#include "platform.h"

#if (MYRTOS_SERVICE_SHELL_ENABLE == 1)

#include <stdio.h>
#include <string.h>

#include "MyRTOS_IO.h"
#include "MyRTOS_Monitor.h"
#include "MyRTOS_Port.h"
#include "MyRTOS_Shell_Private.h"
#include "core_cm4.h"
#include "platform_hires_timer.h"
extern ShellHandle_t g_platform_shell_handle;

// --- 命令实现函数 ---
static int cmd_help(ShellHandle_t shell_h, int argc, char *argv[]);

static int cmd_reboot(ShellHandle_t shell_h, int argc, char *argv[]);

static int cmd_ps(ShellHandle_t shell_h, int argc, char *argv[]);

void platform_register_default_commands(ShellHandle_t shell_h) {
    Shell_RegisterCommand(shell_h, "help", "显示所有可用命令", cmd_help);
    Shell_RegisterCommand(shell_h, "reboot", "重启系统", cmd_reboot);
    Shell_RegisterCommand(shell_h, "ps", "显示系统状态", cmd_ps);
}


// help
static int cmd_help(ShellHandle_t shell_h, int argc, char *argv[]) {
    (void) argc;
    (void) argv;
    StreamHandle_t stream = Shell_GetStream(shell_h);
    ShellInstance_t *shell = (ShellInstance_t *) shell_h;

    Stream_Printf(stream, "Available Commands:\n");

    ShellCommandNode_t *node = shell->commands_head;
    while (node != NULL) {
        Stream_Printf(stream, "  %-12s - %s\n", node->name, node->help);
        node = node->next;
    }
    return 0;
}

// reboot
static int cmd_reboot(ShellHandle_t shell_h, int argc, char *argv[]) {
    (void) argc;
    (void) argv;
    Stream_Printf(Shell_GetStream(shell_h), "System rebooting...\n");
    Task_Delay(MS_TO_TICKS(100));
    NVIC_SystemReset();
    return 0;
}

static TaskStats_t g_last_stats[MYRTOS_MAX_CONCURRENT_TASKS] = {0};
static int g_last_stats_count = 0;
static uint32_t g_last_ps_hires_time = 0;
static uint8_t g_is_first_ps_run = 1;


int cmd_ps(ShellHandle_t shell_h, int argc, char *argv[]) {
    (void) argc;
    (void) argv; // 未使用的参数
    StreamHandle_t stream = Shell_GetStream(shell_h);

    const int COL_WIDTH_NAME = 16;
    static const char *const taskStateToStr[] = {"Unused", "Ready", "Delayed", "Blocked"};
    const char *SEPARATOR_LINE =
            "--------------------------------------------------------------------------------------------------";

    TaskStats_t current_stats[MYRTOS_MAX_CONCURRENT_TASKS];
    int current_stats_count = 0;

    // (这里使用了临界区，因为我们要遍历一个可能在中断中被修改的链表)
    MyRTOS_Port_EnterCritical();
    TaskHandle_t task_h = Monitor_GetNextTask(NULL);
    while (task_h != NULL && current_stats_count < MYRTOS_MAX_CONCURRENT_TASKS) {
        // Monitor_GetTaskInfo 现在是安全的，因为它内部的临界区很短
        if (Monitor_GetTaskInfo(task_h, &current_stats[current_stats_count]) == 0) {
            current_stats_count++;
        }
        task_h = Monitor_GetNextTask(task_h);
    }
    MyRTOS_Port_ExitCritical();

    // 获取当前高精度时间，并计算总时间增量
    uint32_t now_hires = Platform_Timer_GetHiresValue();
    uint32_t total_hires_delta;

    if (g_is_first_ps_run) {
        // 首次运行时，总时间增量没有意义，但需要一个非零的分母
        total_hires_delta = 1;
    } else {
        if (now_hires >= g_last_ps_hires_time) {
            total_hires_delta = now_hires - g_last_ps_hires_time;
        } else {
            // 正确处理32位计时器回绕
            total_hires_delta = (0xFFFFFFFF - g_last_ps_hires_time) + now_hires + 1;
        }
        // 防止除零错误
        if (total_hires_delta == 0) {
            total_hires_delta = 1;
        }
    }

    // 计算任务CPU使用率
    uint64_t total_task_runtime_delta = 0;
    TaskStats_t *idle_task_stats = NULL;

    for (int i = 0; i < current_stats_count; ++i) {
        uint64_t last_task_runtime = 0;
        // 查找该任务上一次的统计快照
        for (int j = 0; j < g_last_stats_count; ++j) {
            if (current_stats[i].task_handle == g_last_stats[j].task_handle) {
                last_task_runtime = g_last_stats[j].total_runtime;
                break;
            }
        }

        uint64_t task_runtime_delta = current_stats[i].total_runtime - last_task_runtime;

        // 使用千分比 (permille) 进行计算以提高精度
        current_stats[i].cpu_usage_permille = (uint32_t) ((task_runtime_delta * 1000) / total_hires_delta);

        // 累加所有任务的运行时间增量，用于计算总CPU使用率
        total_task_runtime_delta += task_runtime_delta;

        // 找到空闲任务的统计信息
        if (current_stats[i].task_handle == idleTask) {
            idle_task_stats = &current_stats[i];
        }
    }

    // 打印表头 ---
    Stream_Printf(stream, "\nMyRTOS Monitor (Uptime: %llu ms)\n", TICK_TO_MS(MyRTOS_GetTick()));
    Stream_Printf(stream, "%-*s %-4s %-8s %-10s %-18s %-8s %s\n", COL_WIDTH_NAME, "Task Name", "ID", "State",
                  "Prio(B/C)", "Stack (Used/Size)", "CPU%", "Runtime(us)");
    Stream_Printf(stream, "%s\n", SEPARATOR_LINE);

    // 打印每个任务的信息
    for (int i = 0; i < current_stats_count; ++i) {
        TaskStats_t *s = &current_stats[i];
        char prio_str[12];
        char stack_str[20];
        snprintf(prio_str, sizeof(prio_str), "%u/%u", s->base_priority, s->current_priority);
        snprintf(stack_str, sizeof(stack_str), "%u/%u", (unsigned) s->stack_high_water_mark_bytes,
                 (unsigned) s->stack_size_bytes);

        // 将高精度时钟的原始tick转换为微秒(us)
        // 假设 platform_get_hires_timer_value 的频率是 1MHz (1 tick = 1 us)
        uint64_t runtime_us = s->total_runtime;

        if (g_is_first_ps_run) {
            Stream_Printf(stream, "%-*s %-4u %-8s %-10s %-18s %-8s %llu\n", COL_WIDTH_NAME, s->task_name,
                          (unsigned) Task_GetId(s->task_handle), taskStateToStr[s->state], prio_str, stack_str,
                          "n/a", // 首次运行不计算CPU占用率
                          runtime_us);
        } else {
            Stream_Printf(stream, "%-*s %-4u %-8s %-10s %-18s %3u.%-1u    %llu\n", COL_WIDTH_NAME, s->task_name,
                          (unsigned) Task_GetId(s->task_handle), taskStateToStr[s->state], prio_str, stack_str,
                          s->cpu_usage_permille / 10, s->cpu_usage_permille % 10, // 打印为 xx.x%
                          runtime_us);
        }
    }

    Stream_Printf(stream, "%s\n", SEPARATOR_LINE);

    // 打印堆信息和总CPU使用率
    HeapStats_t heap;
    Monitor_GetHeapStats(&heap);
    Stream_Printf(stream, "Heap Info -> Total: %-5u | Free: %-5u | Min Ever Free: %-5u\n",
                  (unsigned) heap.total_heap_size, (unsigned) heap.free_bytes_remaining,
                  (unsigned) heap.minimum_ever_free_bytes);

    if (!g_is_first_ps_run) {
        // 通过空闲任务的CPU占用率来反推总的繁忙率
        uint32_t idle_permille = 0;
        if (idle_task_stats != NULL) {
            idle_permille = idle_task_stats->cpu_usage_permille;
        }
        if (idle_permille > 1000)
            idle_permille = 1000; // 防止计算误差超过100%

        uint32_t busy_permille = 1000 - idle_permille;

        Stream_Printf(stream, "CPU Usage -> Total Busy: %u.%u%% | System Idle: %u.%u%%\n", busy_permille / 10,
                      busy_permille % 10, idle_permille / 10, idle_permille % 10);
    }

    // 保存当前状态，作为下一次计算的基准
    memcpy(g_last_stats, current_stats, sizeof(TaskStats_t) * current_stats_count);
    g_last_stats_count = current_stats_count;
    g_last_ps_hires_time = now_hires;
    g_is_first_ps_run = 0; // 清除首次运行标记

    return 0;
}


/**
 * @brief 向平台注册一个或多个用户自定义的Shell命令。
 */
int Platform_RegisterShellCommands(const struct ShellCommand_t *commands, size_t command_count) {
    // 强制转换类型
    const ShellCommand_t *cmd_array = (const ShellCommand_t *) commands;
    int result = 0;

    if (g_platform_shell_handle == NULL) {
        // Shell 服务未初始化或未使能
        return -1;
    }

    for (size_t i = 0; i < command_count; ++i) {
        // 逐个调用底层的动态注册函数
        result = Shell_RegisterCommand(g_platform_shell_handle, cmd_array[i].name, cmd_array[i].help,
                                       cmd_array[i].callback);
        if (result != 0) {
            // 如果其中一个命令注册失败，立即停止并返回错误代码
            // -2: 命令已存在, -3: 内存不足
            return result;
        }
    }
    return 0; // 所有命令都成功注册
}


#endif // MYRTOS_SERVICE_SHELL_ENABLE
