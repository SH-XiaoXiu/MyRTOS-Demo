#include "MyRTOS_Monitor.h"

#if MYRTOS_SERVICE_MONITOR_ENABLE == 1

#include "MyRTOS_Extension.h"
#include "MyRTOS_Port.h"
#include <string.h>

#include "MyRTOS_Kernel_Private.h"

/*===========================================================================*
 *                              内部数据结构                                  *
 *===========================================================================*/

// 内部结构体，仅用于保存动态收集的运行时数据。
typedef struct {
    TaskHandle_t task_handle;
    volatile uint64_t runtime_counter;
} InternalTaskStats_t;


/*===========================================================================*
 *                              模块级全局变量                                *
 *===========================================================================*/

static InternalTaskStats_t g_task_stats_map[MYRTOS_MAX_CONCURRENT_TASKS];
static MonitorGetHiresTimerValueFn g_get_hires_timer_value = NULL;
static volatile uint32_t g_last_switch_time = 0;

// 堆统计信息
static size_t g_min_ever_free_bytes;


/*===========================================================================*
 *                              私有函数                                      *
 *===========================================================================*/

// 查找任务的现有插槽或为新任务分配一个插槽。
static InternalTaskStats_t *find_or_alloc_stat_slot(TaskHandle_t task_h) {
    // 首先，尝试为给定的任务句柄找到现有的插槽
    for (int i = 0; i < MYRTOS_MAX_CONCURRENT_TASKS; ++i) {
        if (g_task_stats_map[i].task_handle == task_h) {
            return &g_task_stats_map[i];
        }
    }
    // 如果 task_h 为 NULL，表示我们要为新任务找一个空插槽
    if (task_h == NULL) {
        for (int i = 0; i < MYRTOS_MAX_CONCURRENT_TASKS; ++i) {
            if (g_task_stats_map[i].task_handle == NULL) {
                return &g_task_stats_map[i];
            }
        }
    }
    return NULL; // 未找到插槽
}

// 内核事件处理函数，被动收集所有监控数据。
static void monitor_kernel_event_handler(const KernelEventData_t *pEventData) {
    // 运行时统计需要高分辨率计时器。
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
                slot->runtime_counter += delta_hires; // 将正确的32位增量累加到64位计数器上
            }
            break;
        }

        case KERNEL_EVENT_TASK_SWITCH_IN: {
            g_last_switch_time = g_get_hires_timer_value();
            break;
        }
        case KERNEL_EVENT_MALLOC:
        case KERNEL_EVENT_FREE: {
            // 在任何内存操作后，检查是否达到了新的最低水位线。
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
 *                              公共API实现                                   *
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
    // 转换为内部 TCB 类型以遍历内核的私有任务列表。
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

    // ====================== 关键修改 ======================
    // 为了安全，先在临界区之外准备好需要的信息
    StackType_t *local_stack_base;
    uint16_t local_stack_size_words;

    // --- 步骤 1: 进入一个极短的临界区，只用来复制指针和大小 ---
    MyRTOS_Port_EnterCritical();
    {
        // 直接从 TCB 填充静态信息
        p_stats_out->task_handle = task_h;
        p_stats_out->task_name = tcb->taskName;
        p_stats_out->state = tcb->state;
        p_stats_out->current_priority = tcb->priority;
        p_stats_out->base_priority = tcb->basePriority;
        p_stats_out->stack_size_bytes = tcb->stackSize_words * sizeof(StackType_t);

        // 从收集的统计信息中填充运行时信息
        InternalTaskStats_t *run_stats = find_or_alloc_stat_slot(task_h);
        if (run_stats) {
            p_stats_out->total_runtime = run_stats->runtime_counter;
        } else {
            p_stats_out->total_runtime = 0;
        }

        // 把需要进行耗时操作的数据复制到局部变量中
        local_stack_base = tcb->stack_base;
        local_stack_size_words = tcb->stackSize_words;
    }
    MyRTOS_Port_ExitCritical(); // --- 立刻退出临界区，重新开启中断 ---


    // --- 步骤 2: 在临界区之外，安全地执行耗时的堆栈扫描 ---
    // 即使这里的指针是错误的，导致死循环，也只会卡住`Shell_Task`自己，
    // 而不会因为关中断而冻结整个系统。SysTick会继续运行。
    uint32_t unused_words = 0;
    if (local_stack_base != NULL) { // 增加一个安全检查
        StackType_t *stack_ptr = local_stack_base;
        while (unused_words < local_stack_size_words && *stack_ptr == 0xA5A5A5A5) {
            unused_words++;
            stack_ptr++;
        }
    }

    // 已使用部分是总大小减去未使用部分
    p_stats_out->stack_high_water_mark_bytes = (local_stack_size_words - unused_words) * sizeof(StackType_t);

    p_stats_out->cpu_usage_permille = 0; // 这个在ps命令里计算

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
