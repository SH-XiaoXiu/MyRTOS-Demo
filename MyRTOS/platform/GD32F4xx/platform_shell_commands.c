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

// --- ����ʵ�ֺ��� ---
static int cmd_help(ShellHandle_t shell_h, int argc, char *argv[]);

static int cmd_reboot(ShellHandle_t shell_h, int argc, char *argv[]);

static int cmd_ps(ShellHandle_t shell_h, int argc, char *argv[]);

void platform_register_default_commands(ShellHandle_t shell_h) {
    Shell_RegisterCommand(shell_h, "help", "��ʾ���п�������", cmd_help);
    Shell_RegisterCommand(shell_h, "reboot", "����ϵͳ", cmd_reboot);
    Shell_RegisterCommand(shell_h, "ps", "��ʾϵͳ״̬", cmd_ps);
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
    (void) argv; // δʹ�õĲ���
    StreamHandle_t stream = Shell_GetStream(shell_h);

    const int COL_WIDTH_NAME = 16;
    static const char *const taskStateToStr[] = {"Unused", "Ready", "Delayed", "Blocked"};
    const char *SEPARATOR_LINE =
            "--------------------------------------------------------------------------------------------------";

    TaskStats_t current_stats[MYRTOS_MAX_CONCURRENT_TASKS];
    int current_stats_count = 0;

    // (����ʹ�����ٽ�������Ϊ����Ҫ����һ���������ж��б��޸ĵ�����)
    MyRTOS_Port_EnterCritical();
    TaskHandle_t task_h = Monitor_GetNextTask(NULL);
    while (task_h != NULL && current_stats_count < MYRTOS_MAX_CONCURRENT_TASKS) {
        // Monitor_GetTaskInfo �����ǰ�ȫ�ģ���Ϊ���ڲ����ٽ����ܶ�
        if (Monitor_GetTaskInfo(task_h, &current_stats[current_stats_count]) == 0) {
            current_stats_count++;
        }
        task_h = Monitor_GetNextTask(task_h);
    }
    MyRTOS_Port_ExitCritical();

    // ��ȡ��ǰ�߾���ʱ�䣬��������ʱ������
    uint32_t now_hires = Platform_Timer_GetHiresValue();
    uint32_t total_hires_delta;

    if (g_is_first_ps_run) {
        // �״�����ʱ����ʱ������û�����壬����Ҫһ������ķ�ĸ
        total_hires_delta = 1;
    } else {
        if (now_hires >= g_last_ps_hires_time) {
            total_hires_delta = now_hires - g_last_ps_hires_time;
        } else {
            // ��ȷ����32λ��ʱ������
            total_hires_delta = (0xFFFFFFFF - g_last_ps_hires_time) + now_hires + 1;
        }
        // ��ֹ�������
        if (total_hires_delta == 0) {
            total_hires_delta = 1;
        }
    }

    // ��������CPUʹ����
    uint64_t total_task_runtime_delta = 0;
    TaskStats_t *idle_task_stats = NULL;

    for (int i = 0; i < current_stats_count; ++i) {
        uint64_t last_task_runtime = 0;
        // ���Ҹ�������һ�ε�ͳ�ƿ���
        for (int j = 0; j < g_last_stats_count; ++j) {
            if (current_stats[i].task_handle == g_last_stats[j].task_handle) {
                last_task_runtime = g_last_stats[j].total_runtime;
                break;
            }
        }

        uint64_t task_runtime_delta = current_stats[i].total_runtime - last_task_runtime;

        // ʹ��ǧ�ֱ� (permille) ���м�������߾���
        current_stats[i].cpu_usage_permille = (uint32_t) ((task_runtime_delta * 1000) / total_hires_delta);

        // �ۼ��������������ʱ�����������ڼ�����CPUʹ����
        total_task_runtime_delta += task_runtime_delta;

        // �ҵ����������ͳ����Ϣ
        if (current_stats[i].task_handle == idleTask) {
            idle_task_stats = &current_stats[i];
        }
    }

    // ��ӡ��ͷ ---
    Stream_Printf(stream, "\nMyRTOS Monitor (Uptime: %llu ms)\n", TICK_TO_MS(MyRTOS_GetTick()));
    Stream_Printf(stream, "%-*s %-4s %-8s %-10s %-18s %-8s %s\n", COL_WIDTH_NAME, "Task Name", "ID", "State",
                  "Prio(B/C)", "Stack (Used/Size)", "CPU%", "Runtime(us)");
    Stream_Printf(stream, "%s\n", SEPARATOR_LINE);

    // ��ӡÿ���������Ϣ
    for (int i = 0; i < current_stats_count; ++i) {
        TaskStats_t *s = &current_stats[i];
        char prio_str[12];
        char stack_str[20];
        snprintf(prio_str, sizeof(prio_str), "%u/%u", s->base_priority, s->current_priority);
        snprintf(stack_str, sizeof(stack_str), "%u/%u", (unsigned) s->stack_high_water_mark_bytes,
                 (unsigned) s->stack_size_bytes);

        // ���߾���ʱ�ӵ�ԭʼtickת��Ϊ΢��(us)
        // ���� platform_get_hires_timer_value ��Ƶ���� 1MHz (1 tick = 1 us)
        uint64_t runtime_us = s->total_runtime;

        if (g_is_first_ps_run) {
            Stream_Printf(stream, "%-*s %-4u %-8s %-10s %-18s %-8s %llu\n", COL_WIDTH_NAME, s->task_name,
                          (unsigned) Task_GetId(s->task_handle), taskStateToStr[s->state], prio_str, stack_str,
                          "n/a", // �״����в�����CPUռ����
                          runtime_us);
        } else {
            Stream_Printf(stream, "%-*s %-4u %-8s %-10s %-18s %3u.%-1u    %llu\n", COL_WIDTH_NAME, s->task_name,
                          (unsigned) Task_GetId(s->task_handle), taskStateToStr[s->state], prio_str, stack_str,
                          s->cpu_usage_permille / 10, s->cpu_usage_permille % 10, // ��ӡΪ xx.x%
                          runtime_us);
        }
    }

    Stream_Printf(stream, "%s\n", SEPARATOR_LINE);

    // ��ӡ����Ϣ����CPUʹ����
    HeapStats_t heap;
    Monitor_GetHeapStats(&heap);
    Stream_Printf(stream, "Heap Info -> Total: %-5u | Free: %-5u | Min Ever Free: %-5u\n",
                  (unsigned) heap.total_heap_size, (unsigned) heap.free_bytes_remaining,
                  (unsigned) heap.minimum_ever_free_bytes);

    if (!g_is_first_ps_run) {
        // ͨ�����������CPUռ�����������ܵķ�æ��
        uint32_t idle_permille = 0;
        if (idle_task_stats != NULL) {
            idle_permille = idle_task_stats->cpu_usage_permille;
        }
        if (idle_permille > 1000)
            idle_permille = 1000; // ��ֹ��������100%

        uint32_t busy_permille = 1000 - idle_permille;

        Stream_Printf(stream, "CPU Usage -> Total Busy: %u.%u%% | System Idle: %u.%u%%\n", busy_permille / 10,
                      busy_permille % 10, idle_permille / 10, idle_permille % 10);
    }

    // ���浱ǰ״̬����Ϊ��һ�μ���Ļ�׼
    memcpy(g_last_stats, current_stats, sizeof(TaskStats_t) * current_stats_count);
    g_last_stats_count = current_stats_count;
    g_last_ps_hires_time = now_hires;
    g_is_first_ps_run = 0; // ����״����б��

    return 0;
}


/**
 * @brief ��ƽ̨ע��һ�������û��Զ����Shell���
 */
int Platform_RegisterShellCommands(const struct ShellCommand_t *commands, size_t command_count) {
    // ǿ��ת������
    const ShellCommand_t *cmd_array = (const ShellCommand_t *) commands;
    int result = 0;

    if (g_platform_shell_handle == NULL) {
        // Shell ����δ��ʼ����δʹ��
        return -1;
    }

    for (size_t i = 0; i < command_count; ++i) {
        // ������õײ�Ķ�̬ע�ắ��
        result = Shell_RegisterCommand(g_platform_shell_handle, cmd_array[i].name, cmd_array[i].help,
                                       cmd_array[i].callback);
        if (result != 0) {
            // �������һ������ע��ʧ�ܣ�����ֹͣ�����ش������
            // -2: �����Ѵ���, -3: �ڴ治��
            return result;
        }
    }
    return 0; // ��������ɹ�ע��
}


#endif // MYRTOS_SERVICE_SHELL_ENABLE
