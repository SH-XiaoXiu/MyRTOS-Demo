/**
 * @file  MyRTOS_Timer.c
 * @brief MyRTOS �����ʱ������ - ʵ��
 */
#include "MyRTOS_Timer.h"

#if MYRTOS_SERVICE_TIMER_ENABLE == 1

#include <stdbool.h>
#include <string.h>
#include "MyRTOS.h"

/*============================== �ڲ����ݽṹ ==============================*/

// ��ʱ�����ƿ�
typedef struct Timer_t {
    const char *name;
    TimerCallback_t callback;
    void *p_timer_arg;
    uint32_t period;
    volatile uint64_t expiry_time; // �´ε���ʱ�ľ���ϵͳtick
    uint8_t is_periodic;
    volatile bool is_active;
    struct Timer_t *p_next; // ���ڹ�������Ļ��ʱ������
} Timer_t;

// ���͸���ʱ�������������������
typedef enum { TIMER_CMD_START, TIMER_CMD_STOP, TIMER_CMD_DELETE, TIMER_CMD_CHANGE_PERIOD } TimerCommandType_t;

// ����ṹ��
typedef struct {
    TimerCommandType_t command;
    TimerHandle_t timer;
    uint32_t value; // ���ڴ��������ڵ�ֵ
} TimerCommand_t;

/*============================== ģ��ȫ�ֱ��� ==============================*/

static TaskHandle_t g_timer_service_task_handle = NULL;
static QueueHandle_t g_timer_command_queue = NULL;
// ���ʱ������ͷ��������ʱ����������
static TimerHandle_t g_active_timer_list_head = NULL;

/*============================== ˽�к���ԭ�� ==============================*/

static void TimerServiceTask(void *pv);

static void process_timer_command(const TimerCommand_t *command);

static void insert_timer_into_active_list(TimerHandle_t timer_to_insert);

static int send_command_to_timer_task(TimerHandle_t timer, TimerCommandType_t cmd, uint32_t value,
                                      uint32_t block_ticks);

/*=========================== ��ʱ������������غ��� ===========================*/

// ��ʱ������������ѭ��
static void TimerServiceTask(void *pv) {
    (void) pv;
    TimerCommand_t command;

    while (1) {
        uint64_t now = MyRTOS_GetTick();
        uint32_t block_ticks;

        // ���ݵ�ǰ�����״̬��������һ����Ҫ���ѵ�ʱ��
        if (g_active_timer_list_head == NULL) {
            block_ticks = MYRTOS_MAX_DELAY; // �޻��ʱ��������������
        } else if (g_active_timer_list_head->expiry_time <= now) {
            block_ticks = 0; // ����Ķ�ʱ���ѵ��ڣ���������������
        } else {
            block_ticks = (uint32_t) (g_active_timer_list_head->expiry_time - now);
        }

        // �����ȴ�������У���ȴ�ֱ������Ķ�ʱ������
        if (Queue_Receive(g_timer_command_queue, &command, block_ticks) == 1) {
            // �ɹ��յ�һ�������������
            process_timer_command(&command);
            // ������鲢��������п��ܻ�ѹ��������������ӳ�
            while (Queue_Receive(g_timer_command_queue, &command, 0) == 1) {
                process_timer_command(&command);
            }
        }

        // ���������ѵ��ڵĶ�ʱ��
        now = MyRTOS_GetTick();
        while (g_active_timer_list_head != NULL && g_active_timer_list_head->expiry_time <= now) {
            TimerHandle_t expired_timer = g_active_timer_list_head;
            // �ӻ�������Ƴ�
            g_active_timer_list_head = expired_timer->p_next;
            expired_timer->is_active = false;

            // ִ�лص�
            if (expired_timer->callback) {
                expired_timer->callback(expired_timer);
            }

            // ����������Զ�ʱ�������¼����´ε���ʱ�䲢�������
            if (expired_timer->is_periodic) {
                // �����۵���ʱ��������һ�Σ������ۻ����
                expired_timer->expiry_time += expired_timer->period;
                insert_timer_into_active_list(expired_timer);
            }
        }
    }
}

// ����Ӷ������յ��ĵ�������
static void process_timer_command(const TimerCommand_t *command) {
    TimerHandle_t timer = command->timer;
    if (!timer)
        return;

    bool was_active = timer->is_active;

    // �ӻ�������Ƴ��������ٸ�����������Ƿ����²���
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
            // �Ѿ��������Ƴ��ˣ�����������
            break;
        case TIMER_CMD_DELETE:
            MyRTOS_Free(timer); // �ͷŶ�ʱ���ڴ�
            break;
        case TIMER_CMD_CHANGE_PERIOD:
            timer->period = command->value;
            // ���֮ǰ�ǻ�ģ��������������¼�����
            if (was_active) {
                timer->expiry_time = MyRTOS_GetTick() + timer->period;
                insert_timer_into_active_list(timer);
            }
            break;
    }
}

// ��һ����ʱ��������ʱ��˳����뵽�������
static void insert_timer_into_active_list(TimerHandle_t timer_to_insert) {
    timer_to_insert->is_active = true;
    if (g_active_timer_list_head == NULL || timer_to_insert->expiry_time < g_active_timer_list_head->expiry_time) {
        // ���뵽����ͷ��
        timer_to_insert->p_next = g_active_timer_list_head;
        g_active_timer_list_head = timer_to_insert;
    } else {
        // ���������ҵ����ʵĲ���λ��
        Timer_t *iterator = g_active_timer_list_head;
        while (iterator->p_next && iterator->p_next->expiry_time <= timer_to_insert->expiry_time) {
            iterator = iterator->p_next;
        }
        timer_to_insert->p_next = iterator->p_next;
        iterator->p_next = timer_to_insert;
    }
}

/*============================== ����APIʵ�� ===============================*/

int TimerService_Init(uint8_t timer_task_priority, uint16_t timer_task_stack_size) {
    if (g_timer_service_task_handle != NULL)
        return 0; // ��ֹ�ظ���ʼ��

    // ����������У�������ȿ�����
    g_timer_command_queue = Queue_Create(MYRTOS_TIMER_COMMAND_QUEUE_SIZE, sizeof(TimerCommand_t));
    if (g_timer_command_queue == NULL)
        return -1;

    // ������ʱ����������
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
        timer->period = (period == 0) ? 1 : period; // ��������Ϊ1 tick
        timer->is_periodic = is_periodic;
        timer->callback = callback;
        timer->p_timer_arg = p_timer_arg;
        timer->is_active = false;
        timer->p_next = NULL;
    }
    return timer;
}

// ͳһ������ͺ���
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
