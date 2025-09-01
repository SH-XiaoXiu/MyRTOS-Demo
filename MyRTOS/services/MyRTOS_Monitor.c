#include "MyRTOS_Monitor.h"

#if MYRTOS_SERVICE_MONITOR_ENABLE == 1

#include "MyRTOS_Extension.h"
#include "MyRTOS_Port.h"
#include <string.h>

#include "MyRTOS_Kernel_Private.h"

/*===========================================================================*
 *                              �ڲ����ݽṹ                                  *
 *===========================================================================*/

// �ڲ��ṹ�壬�����ڱ��涯̬�ռ�������ʱ���ݡ�
typedef struct {
    TaskHandle_t task_handle;
    volatile uint64_t runtime_counter;
} InternalTaskStats_t;


/*===========================================================================*
 *                              ģ�鼶ȫ�ֱ���                                *
 *===========================================================================*/

static InternalTaskStats_t g_task_stats_map[MYRTOS_MAX_CONCURRENT_TASKS];
static MonitorGetHiresTimerValueFn g_get_hires_timer_value = NULL;
static volatile uint32_t g_last_switch_time = 0;

// ��ͳ����Ϣ
static size_t g_min_ever_free_bytes;


/*===========================================================================*
 *                              ˽�к���                                      *
 *===========================================================================*/

// ������������в�ۻ�Ϊ���������һ����ۡ�
static InternalTaskStats_t *find_or_alloc_stat_slot(TaskHandle_t task_h) {
    // ���ȣ�����Ϊ�������������ҵ����еĲ��
    for (int i = 0; i < MYRTOS_MAX_CONCURRENT_TASKS; ++i) {
        if (g_task_stats_map[i].task_handle == task_h) {
            return &g_task_stats_map[i];
        }
    }
    // ��� task_h Ϊ NULL����ʾ����ҪΪ��������һ���ղ��
    if (task_h == NULL) {
        for (int i = 0; i < MYRTOS_MAX_CONCURRENT_TASKS; ++i) {
            if (g_task_stats_map[i].task_handle == NULL) {
                return &g_task_stats_map[i];
            }
        }
    }
    return NULL; // δ�ҵ����
}

// �ں��¼��������������ռ����м�����ݡ�
static void monitor_kernel_event_handler(const KernelEventData_t *pEventData) {
    // ����ʱͳ����Ҫ�߷ֱ��ʼ�ʱ����
    if (g_get_hires_timer_value == NULL &&
        (pEventData->eventType == KERNEL_EVENT_TASK_SWITCH_OUT || pEventData->eventType ==
         KERNEL_EVENT_TASK_SWITCH_IN)) {
        return;
    }

    switch (pEventData->eventType) {
        case KERNEL_EVENT_TASK_CREATE: {
            InternalTaskStats_t *slot = find_or_alloc_stat_slot(NULL);
            if (slot) {
                slot->task_handle = pEventData->task;
                slot->runtime_counter = 0;
            }
            break;
        }
        case KERNEL_EVENT_TASK_DELETE: {
            InternalTaskStats_t *slot = find_or_alloc_stat_slot(pEventData->task);
            if (slot) {
                slot->task_handle = NULL;
            }
            break;
        }

        case KERNEL_EVENT_TASK_SWITCH_OUT: {
            // ==================== FIX START ====================
            uint32_t now_hires = g_get_hires_timer_value();
            uint32_t last_hires = (uint32_t)g_last_switch_time;
            uint32_t delta_hires;

            if (now_hires >= last_hires) {
                delta_hires = now_hires - last_hires;
            } else {
                delta_hires = (0xFFFFFFFF - last_hires) + now_hires + 1;
            }

            InternalTaskStats_t *slot = find_or_alloc_stat_slot(pEventData->task);
            if (slot) {
                slot->runtime_counter += delta_hires; // ����ȷ��32λ�����ۼӵ�64λ��������
            }
            break;
        }

        case KERNEL_EVENT_TASK_SWITCH_IN: {
            g_last_switch_time = g_get_hires_timer_value();
            break;
        }
        case KERNEL_EVENT_MALLOC:
        case KERNEL_EVENT_FREE: {
            // ���κ��ڴ�����󣬼���Ƿ�ﵽ���µ����ˮλ�ߡ�
            if (freeBytesRemaining < g_min_ever_free_bytes) {
                g_min_ever_free_bytes = freeBytesRemaining;
            }
            break;
        }

        default:
            break;
    }
}

/*===========================================================================*
 *                              ����APIʵ��                                   *
 *===========================================================================*/

int Monitor_Init(const MonitorConfig_t *config) {
    if (!config || !config->get_hires_timer_value) {
        return -1;
    }
    memset(g_task_stats_map, 0, sizeof(g_task_stats_map));
    g_get_hires_timer_value = config->get_hires_timer_value;

    MyRTOS_Port_EnterCritical();
    g_min_ever_free_bytes = freeBytesRemaining == 0 ? MYRTOS_MEMORY_POOL_SIZE : freeBytesRemaining;
    g_last_switch_time = g_get_hires_timer_value();
    MyRTOS_Port_ExitCritical();

    return MyRTOS_RegisterExtension(monitor_kernel_event_handler);
}

TaskHandle_t Monitor_GetNextTask(TaskHandle_t previous_handle) {
    // ת��Ϊ�ڲ� TCB �����Ա����ں˵�˽�������б�
    Task_t *prev_tcb = (Task_t *) previous_handle;
    if (prev_tcb == NULL) {
        return allTaskListHead;
    }
    return prev_tcb->pNextTask;
}

int Monitor_GetTaskInfo(TaskHandle_t task_h, TaskStats_t *p_stats_out) {
    if (task_h == NULL || p_stats_out == NULL) {
        return -1;
    }

    Task_t *tcb = (Task_t *) task_h;

    // ====================== �ؼ��޸� ======================
    // Ϊ�˰�ȫ�������ٽ���֮��׼������Ҫ����Ϣ
    StackType_t *local_stack_base;
    uint16_t local_stack_size_words;

    // --- ���� 1: ����һ�����̵��ٽ�����ֻ��������ָ��ʹ�С ---
    MyRTOS_Port_EnterCritical();
    {
        // ֱ�Ӵ� TCB ��侲̬��Ϣ
        p_stats_out->task_handle = task_h;
        p_stats_out->task_name = tcb->taskName;
        p_stats_out->state = tcb->state;
        p_stats_out->current_priority = tcb->priority;
        p_stats_out->base_priority = tcb->basePriority;
        p_stats_out->stack_size_bytes = tcb->stackSize_words * sizeof(StackType_t);

        // ���ռ���ͳ����Ϣ���������ʱ��Ϣ
        InternalTaskStats_t *run_stats = find_or_alloc_stat_slot(task_h);
        if (run_stats) {
            p_stats_out->total_runtime = run_stats->runtime_counter;
        } else {
            p_stats_out->total_runtime = 0;
        }

        // ����Ҫ���к�ʱ���������ݸ��Ƶ��ֲ�������
        local_stack_base = tcb->stack_base;
        local_stack_size_words = tcb->stackSize_words;
    }
    MyRTOS_Port_ExitCritical(); // --- �����˳��ٽ��������¿����ж� ---


    // --- ���� 2: ���ٽ���֮�⣬��ȫ��ִ�к�ʱ�Ķ�ջɨ�� ---
    // ��ʹ�����ָ���Ǵ���ģ�������ѭ����Ҳֻ�Ῠס`Shell_Task`�Լ���
    // ��������Ϊ���ж϶���������ϵͳ��SysTick��������С�
    uint32_t unused_words = 0;
    if (local_stack_base != NULL) { // ����һ����ȫ���
        StackType_t *stack_ptr = local_stack_base;
        while (unused_words < local_stack_size_words && *stack_ptr == 0xA5A5A5A5) {
            unused_words++;
            stack_ptr++;
        }
    }

    // ��ʹ�ò������ܴ�С��ȥδʹ�ò���
    p_stats_out->stack_high_water_mark_bytes = (local_stack_size_words - unused_words) * sizeof(StackType_t);

    p_stats_out->cpu_usage_permille = 0; // �����ps���������

    return 0;
}

void Monitor_GetHeapStats(HeapStats_t *p_stats_out) {
    if (p_stats_out == NULL) return;
    MyRTOS_Port_EnterCritical();
    p_stats_out->total_heap_size = MYRTOS_MEMORY_POOL_SIZE;
    p_stats_out->free_bytes_remaining = freeBytesRemaining;
    p_stats_out->minimum_ever_free_bytes = g_min_ever_free_bytes;
    MyRTOS_Port_ExitCritical();
}
#endif
