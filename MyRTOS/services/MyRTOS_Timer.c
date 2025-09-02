/**
 * @file  MyRTOS_Timer.c
 * @brief MyRTOS 软件定时器服务 - 实现
 */
#include "MyRTOS_Timer.h"

#if MYRTOS_SERVICE_TIMER_ENABLE == 1

#include <stdbool.h>
#include <string.h>
#include "MyRTOS.h"

/*============================== 内部数据结构 ==============================*/

// 定时器控制块
typedef struct Timer_t {
    const char *name;
    TimerCallback_t callback;
    void *p_timer_arg;
    uint32_t period;
    volatile uint64_t expiry_time; // 下次到期时的绝对系统tick
    uint8_t is_periodic;
    volatile bool is_active;
    struct Timer_t *p_next; // 用于构建有序的活动定时器链表
} Timer_t;

// 发送给定时器服务任务的命令类型
typedef enum { TIMER_CMD_START, TIMER_CMD_STOP, TIMER_CMD_DELETE, TIMER_CMD_CHANGE_PERIOD } TimerCommandType_t;

// 命令结构体
typedef struct {
    TimerCommandType_t command;
    TimerHandle_t timer;
    uint32_t value; // 用于传递新周期等值
} TimerCommand_t;

/*============================== 模块全局变量 ==============================*/

static TaskHandle_t g_timer_service_task_handle = NULL;
static QueueHandle_t g_timer_command_queue = NULL;
// 活动定时器链表头，按到期时间升序排列
static TimerHandle_t g_active_timer_list_head = NULL;

/*============================== 私有函数原型 ==============================*/

static void TimerServiceTask(void *pv);

static void process_timer_command(const TimerCommand_t *command);

static void insert_timer_into_active_list(TimerHandle_t timer_to_insert);

static int send_command_to_timer_task(TimerHandle_t timer, TimerCommandType_t cmd, uint32_t value,
                                      uint32_t block_ticks);

/*=========================== 定时器服务任务及相关函数 ===========================*/

// 定时器服务任务主循环
static void TimerServiceTask(void *pv) {
    (void) pv;
    TimerCommand_t command;

    while (1) {
        uint64_t now = MyRTOS_GetTick();
        uint32_t block_ticks;

        // 根据当前活动链表状态，计算下一次需要唤醒的时间
        if (g_active_timer_list_head == NULL) {
            block_ticks = MYRTOS_MAX_DELAY; // 无活动定时器，无限期阻塞
        } else if (g_active_timer_list_head->expiry_time <= now) {
            block_ticks = 0; // 最近的定时器已到期，立即处理，不阻塞
        } else {
            block_ticks = (uint32_t) (g_active_timer_list_head->expiry_time - now);
        }

        // 阻塞等待命令队列，或等待直到最近的定时器到期
        if (Queue_Receive(g_timer_command_queue, &command, block_ticks) == 1) {
            // 成功收到一个新命令，处理它
            process_timer_command(&command);
            // 立即检查并处理队列中可能积压的其他命令，避免延迟
            while (Queue_Receive(g_timer_command_queue, &command, 0) == 1) {
                process_timer_command(&command);
            }
        }

        // 处理所有已到期的定时器
        now = MyRTOS_GetTick();
        while (g_active_timer_list_head != NULL && g_active_timer_list_head->expiry_time <= now) {
            TimerHandle_t expired_timer = g_active_timer_list_head;
            // 从活动链表中移除
            g_active_timer_list_head = expired_timer->p_next;
            expired_timer->is_active = false;

            // 执行回调
            if (expired_timer->callback) {
                expired_timer->callback(expired_timer);
            }

            // 如果是周期性定时器，重新计算下次到期时间并插回链表
            if (expired_timer->is_periodic) {
                // 从理论到期时间点计算下一次，避免累积误差
                expired_timer->expiry_time += expired_timer->period;
                insert_timer_into_active_list(expired_timer);
            }
        }
    }
}

// 处理从队列中收到的单个命令
static void process_timer_command(const TimerCommand_t *command) {
    TimerHandle_t timer = command->timer;
    if (!timer)
        return;

    bool was_active = timer->is_active;

    // 从活动链表中移除，后续再根据命令决定是否重新插入
    if (was_active) {
        if (g_active_timer_list_head == timer) {
            g_active_timer_list_head = timer->p_next;
        } else {
            Timer_t *iter = g_active_timer_list_head;
            while (iter && iter->p_next != timer)
                iter = iter->p_next;
            if (iter)
                iter->p_next = timer->p_next;
        }
        timer->is_active = false;
    }

    switch (command->command) {
        case TIMER_CMD_START:
            timer->expiry_time = MyRTOS_GetTick() + timer->period;
            insert_timer_into_active_list(timer);
            break;
        case TIMER_CMD_STOP:
            // 已经在上面移除了，无需额外操作
            break;
        case TIMER_CMD_DELETE:
            MyRTOS_Free(timer); // 释放定时器内存
            break;
        case TIMER_CMD_CHANGE_PERIOD:
            timer->period = command->value;
            // 如果之前是活动的，则用新周期重新激活它
            if (was_active) {
                timer->expiry_time = MyRTOS_GetTick() + timer->period;
                insert_timer_into_active_list(timer);
            }
            break;
    }
}

// 将一个定时器按到期时间顺序插入到活动链表中
static void insert_timer_into_active_list(TimerHandle_t timer_to_insert) {
    timer_to_insert->is_active = true;
    if (g_active_timer_list_head == NULL || timer_to_insert->expiry_time < g_active_timer_list_head->expiry_time) {
        // 插入到链表头部
        timer_to_insert->p_next = g_active_timer_list_head;
        g_active_timer_list_head = timer_to_insert;
    } else {
        // 遍历链表找到合适的插入位置
        Timer_t *iterator = g_active_timer_list_head;
        while (iterator->p_next && iterator->p_next->expiry_time <= timer_to_insert->expiry_time) {
            iterator = iterator->p_next;
        }
        timer_to_insert->p_next = iterator->p_next;
        iterator->p_next = timer_to_insert;
    }
}

/*============================== 公共API实现 ===============================*/

int TimerService_Init(uint8_t timer_task_priority, uint16_t timer_task_stack_size) {
    if (g_timer_service_task_handle != NULL)
        return 0; // 防止重复初始化

    // 创建命令队列，队列深度可配置
    g_timer_command_queue = Queue_Create(MYRTOS_TIMER_COMMAND_QUEUE_SIZE, sizeof(TimerCommand_t));
    if (g_timer_command_queue == NULL)
        return -1;

    // 创建定时器服务任务
    g_timer_service_task_handle =
            Task_Create(TimerServiceTask, "TimerSvc", timer_task_stack_size, NULL, timer_task_priority);
    if (g_timer_service_task_handle == NULL) {
        Queue_Delete(g_timer_command_queue);
        return -1;
    }
    return 0;
}

TimerHandle_t Timer_Create(const char *name, uint32_t period, uint8_t is_periodic, TimerCallback_t callback,
                           void *p_timer_arg) {
    TimerHandle_t timer = (TimerHandle_t) MyRTOS_Malloc(sizeof(Timer_t));
    if (timer) {
        timer->name = name;
        timer->period = (period == 0) ? 1 : period; // 周期至少为1 tick
        timer->is_periodic = is_periodic;
        timer->callback = callback;
        timer->p_timer_arg = p_timer_arg;
        timer->is_active = false;
        timer->p_next = NULL;
    }
    return timer;
}

// 统一的命令发送函数
static int send_command_to_timer_task(TimerHandle_t timer, TimerCommandType_t cmd, uint32_t value,
                                      uint32_t block_ticks) {
    if (g_timer_service_task_handle == NULL || timer == NULL)
        return -1;
    TimerCommand_t command = {.command = cmd, .timer = timer, .value = value};
    return (Queue_Send(g_timer_command_queue, &command, block_ticks) == 1) ? 0 : -1;
}

int Timer_Start(TimerHandle_t timer, uint32_t block_ticks) {
    return send_command_to_timer_task(timer, TIMER_CMD_START, 0, block_ticks);
}

int Timer_Stop(TimerHandle_t timer, uint32_t block_ticks) {
    return send_command_to_timer_task(timer, TIMER_CMD_STOP, 0, block_ticks);
}

int Timer_Delete(TimerHandle_t timer, uint32_t block_ticks) {
    return send_command_to_timer_task(timer, TIMER_CMD_DELETE, 0, block_ticks);
}

int Timer_ChangePeriod(TimerHandle_t timer, uint32_t new_period, uint32_t block_ticks) {
    return send_command_to_timer_task(timer, TIMER_CMD_CHANGE_PERIOD, new_period, block_ticks);
}

void *Timer_GetArg(TimerHandle_t timer) { return timer ? timer->p_timer_arg : NULL; }

#endif
