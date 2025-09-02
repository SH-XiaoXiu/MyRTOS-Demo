/**
 * @file  main.c
 * @brief MyRTOS ʾ������
 * @author XiaoXiu
 * @date  2025-08-31
*/
#include "gd32f4xx_exti.h"
#include "gd32f4xx_gpio.h"
#include "gd32f4xx_misc.h"
#include "gd32f4xx_rcu.h"
#include "gd32f4xx_syscfg.h"
#include "platform.h"
#include "MyRTOS.h"
#include "MyRTOS_IO.h"
#include "MyRTOS_Log.h"
#include "MyRTOS_Port.h"
#include "MyRTOS_Timer.h"

/*===========================================================================*
 *                            �������ȼ�����                                 *
 *===========================================================================*/
#define BACKGROUND_TASK_PRIO       1
#define CONSUMER_PRIO              2
#define PRINTER_TASK_PRIO          2
#define COLLABORATION_TASKS_PRIO   3
#define PRODUCER_PRIO              3
#define TIMER_TEST_TASK_PRIO       3
#define ISR_TEST_TASK_PRIO         4
#define HIGH_PRIO_TASK_PRIO        5
#define INTERRUPT_TASK_PRIO        6
#define INSPECTOR_PRIO             7

/*===========================================================================*
 *                          ȫ�־�������ݽṹ                               *
 *===========================================================================*/
static QueueHandle_t product_queue;
static MutexHandle_t recursive_lock;
static SemaphoreHandle_t printer_semaphore;
static SemaphoreHandle_t isr_semaphore;
static TimerHandle_t perio_timer_h, single_timer_h;

static TaskHandle_t producer_task_h, consumer_task_h, inspector_task_h;
static TaskHandle_t a_task_h, b_task_h, c_task_h, d_task_h;
static TaskHandle_t high_prio_task_h, interrupt_task_h, background_task_h;
static TaskHandle_t recursive_task_h, timer_test_task_h;
static TaskHandle_t printer_task1_h, printer_task2_h, printer_task3_h;
static TaskHandle_t isr_test_task_h;

typedef struct {
    uint32_t id;
    uint32_t data;
} Product_t;

/*===========================================================================*
 *                       ����ͻص�����ʵ��                                  *
 *===========================================================================*/
void perio_timer_cb(TimerHandle_t timer) {
    (void) timer;
    LOG_D("��ʱ���ص�", "�����Զ�ʱ��(10��)����!");
}

void single_timer_cb(TimerHandle_t timer) {
    (void) timer;
    LOG_D("��ʱ���ص�", "һ���Զ�ʱ��(5��)����!");
}

void a_task(void *param) {
    (void) param;
    static uint16_t i = 0;
    while (1) {
        Task_Wait();
        LOG_D("Task A", "�����ѣ���ʼ����...");
        for (i = 1; i <= 5; ++i) {
            LOG_D("Task A", "��������, �� %d ��", i);
            Task_Delay(MS_TO_TICKS(1000));
        }
        i = 0;
        LOG_D("Task A", "������ɣ����� Task B �����µȴ�");
        Task_Notify(b_task_h);
    }
}

void b_task(void *param) {
    (void) param;
    static uint16_t i = 0;
    while (1) {
        Task_Wait();
        LOG_D("Task B", "�����ѣ���ʼ����...");
        for (i = 1; i <= 3; ++i) {
            LOG_D("Task B", "��������, �� %d ��", i);
            Task_Delay(MS_TO_TICKS(1000));
        }
        i = 0;
        LOG_D("Task B", "������ɣ����� Task A �����µȴ�");
        Task_Notify(a_task_h);
    }
}

void c_task(void *param) {
    (void) param;
    uint16_t index = 0;
    LOG_D("Task C", "�Ѵ�������ʼ����.");
    while (1) {
        index++;
        LOG_D("Task C", "��������, �� %d ��", index);
        if (index == 5) {
            LOG_D("Task C", "����5�κ�ɾ���Լ�.");
            MyRTOS_Port_EnterCritical();
            c_task_h = NULL; // ���ٽ����ڰ�ȫ�����ȫ�־��
            MyRTOS_Port_ExitCritical();
            Task_Delete(NULL); // ɾ������
        }
        Task_Delay(MS_TO_TICKS(1000));
    }
}

void d_task(void *param) {
    (void) param;
    while (1) {
        int is_task_c_alive;
        MyRTOS_Port_EnterCritical();
        is_task_c_alive = (c_task_h != NULL);
        MyRTOS_Port_ExitCritical();

        if (!is_task_c_alive) {
            LOG_D("Task D", "��⵽Task C������, ׼�����´���...");
            Task_Delay(MS_TO_TICKS(3000)); // �ȴ�3���ٴ���
            c_task_h = Task_Create(c_task, "TaskC_dynamic", 1024, NULL, COLLABORATION_TASKS_PRIO);
            if (c_task_h == NULL) {
                LOG_E("Task D", "����Task Cʧ��!");
            }
        }
        Task_Delay(MS_TO_TICKS(1000));
    }
}

void background_blinky_task(void *param) {
    (void) param;
    while (1) {
        gpio_bit_toggle(GPIOB, GPIO_PIN_2);
        Task_Delay(MS_TO_TICKS(1000));
    }
}

void high_prio_task(void *param) {
    (void) param;
    while (1) {
        Task_Delay(MS_TO_TICKS(5000));
        LOG_D("�����ȼ�����", "<<<<<<<<<< [��ռ��ʾ] >>>>>>>>>>");
    }
}

void interrupt_handler_task(void *param) {
    (void) param;
    // �������Ĺ���������ƽ̨���ṩ����������Ȼ������������ʾ
    while (1) {
        Task_Wait();
        LOG_D("��������", "�ѱ��жϻ���, ������A����.");
        Task_Notify(a_task_h);
    }
}

void isr_test_task(void *param) {
    (void) param;
    LOG_D("ISR����", "�������ȴ��ź���...");
    while (1) {
        if (Semaphore_Take(isr_semaphore, MYRTOS_MAX_DELAY) == 1) {
            LOG_D("ISR����", "�ɹ��Ӱ����жϻ�ȡ�ź���!");
        }
    }
}

void producer_task(void *param) {
    (void) param;
    Product_t product = {0, 100};
    while (1) {
        product.id++;
        product.data += 10;
        LOG_D("������", "������Ʒ ID %lu", product.id);
        if (Queue_Send(product_queue, &product, MS_TO_TICKS(100)) == 1) {
            LOG_D("������", "��Ʒ ID %lu �ѷ���", product.id);
        } else {
            LOG_D("������", "��������, ���Ͳ�Ʒ ID %lu ʧ��", product.id);
        }
        Task_Delay(MS_TO_TICKS(2000));
    }
}

void consumer_task(void *param) {
    (void) param;
    Product_t received_product;
    while (1) {
        LOG_D("������", "�ȴ���Ʒ...");
        if (Queue_Receive(product_queue, &received_product, MYRTOS_MAX_DELAY) == 1) {
            LOG_D("������", "���յ���Ʒ ID %lu, ����: %lu", received_product.id, received_product.data);
        }
    }
}

void inspector_task(void *param) {
    (void) param;
    Product_t received_product;
    while (1) {
        LOG_D("�ʼ�Ա", "�ȴ����ز�Ʒ...");
        if (Queue_Receive(product_queue, &received_product, MYRTOS_MAX_DELAY) == 1) {
            LOG_D("�ʼ�Ա", "���ص���Ʒ ID %lu, ����!", received_product.id);
        }
        Task_Delay(MS_TO_TICKS(5000));
    }
}

void recursive_test_task(void *param) {
    (void) param;
    while (1) {
        LOG_D("�ݹ���", "��ʼ����...");
        Mutex_Lock_Recursive(recursive_lock);
        LOG_D("�ݹ���", "��ѭ������ (��1��)");
        Task_Delay(MS_TO_TICKS(500));

        Mutex_Lock_Recursive(recursive_lock);
        LOG_D("�ݹ���", "Ƕ�׼��� (��2��)");
        Task_Delay(MS_TO_TICKS(500));
        Mutex_Unlock_Recursive(recursive_lock);
        LOG_D("�ݹ���", "Ƕ�׽��� (��2��)");

        Mutex_Unlock_Recursive(recursive_lock);
        LOG_D("�ݹ���", "��ѭ������ (��1��)");
        LOG_D("�ݹ���", "�������, �ȴ�3��");
        Task_Delay(MS_TO_TICKS(3000));
    }
}

void timer_test_task(void *param) {
    (void) param;
    while (1) {
        uint32_t count = Platform_Timer_GetHiresValue();
        LOG_D("�߾���ʱ��", "��ǰ����ֵ = %lu", count);
        Task_Delay(MS_TO_TICKS(2000));
    }
}

void printer_task(void *param) {
    const char *taskName = (const char *) param;
    while (1) {
        LOG_D(taskName, "���ڵȴ���ӡ��...");
        if (Semaphore_Take(printer_semaphore, MYRTOS_MAX_DELAY) == 1) {
            LOG_D(taskName, "��ȡ����ӡ��, ��ʼ��ӡ (��ʱ3��)...");
            Task_Delay(MS_TO_TICKS(3000));
            LOG_D(taskName, "��ӡ���, �ͷŴ�ӡ��.");
            Semaphore_Give(printer_semaphore);
        }
        Task_Delay(MS_TO_TICKS(500 + (Task_GetId(Task_GetCurrentTaskHandle()) * 300)));
    }
}

/*===========================================================================*
 *                        �û��Զ���Shell����                                *
 *===========================================================================*/
int cmd_114514(ShellHandle_t shell_h, int argc, char *argv[]) {
    (void) argc;
    (void) argv;
    Stream_Printf(Shell_GetStream(shell_h), "�ٺٺ٣�����\n");
    return 0;
}

const ShellCommand_t g_user_commands[] = {
    {"114514", "һ���ܳ���ָ��", cmd_114514},
};
const size_t g_user_command_count = sizeof(g_user_commands) / sizeof(g_user_commands[0]);

/*===========================================================================*
 *                   ƽ̨���Ӻ��� (Platform Hooks)                       *
 *===========================================================================*/

/**
 * @brief ��ʼ����弶��ص�Ӳ�� (BSP)��
 */
void Platform_BSP_Init_Hook(void) {
    // LED GPIO ��ʼ��
    rcu_periph_clock_enable(RCU_GPIOB);
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_2);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_2);

    // �����жϳ�ʼ�� (PA0)
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_SYSCFG);
    gpio_mode_set(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_0);
    syscfg_exti_line_config(EXTI_SOURCE_GPIOA, EXTI_SOURCE_PIN0);
    exti_init(EXTI_0, EXTI_INTERRUPT, EXTI_TRIG_FALLING);
    nvic_irq_enable(EXTI0_IRQn, 5, 0);
}

/**
 * @brief ע���û��Լ���Shell���
 */
void Platform_AppSetup_Hook(ShellHandle_t shell_h) {
    if (shell_h) {
        LOG_D("Hook", "Platform_AppSetup_Hook: Registering user commands...");
        Platform_RegisterShellCommands(g_user_commands, g_user_command_count);
    }
}

/**
 * @brief �������е�Ӧ�ó�������
 */
void Platform_CreateTasks_Hook(void) {
    // LOG_D("Hook", "Platform_CreateTasks_Hook: Creating application tasks...");

    // --- �����ʱ������ ---
    single_timer_h = Timer_Create("���ζ�ʱ��", MS_TO_TICKS(5000), 0, single_timer_cb, NULL);
    perio_timer_h = Timer_Create("���ڶ�ʱ��", MS_TO_TICKS(10000), 1, perio_timer_cb, NULL);
    if (single_timer_h) Timer_Start(single_timer_h, 0);
    if (perio_timer_h) Timer_Start(perio_timer_h, 0);

    // --- ���в��� ---
    product_queue = Queue_Create(3, sizeof(Product_t));
    if (product_queue) {
        consumer_task_h = Task_Create(consumer_task, "Consumer", 256, NULL, CONSUMER_PRIO);
        producer_task_h = Task_Create(producer_task, "Producer", 256, NULL, PRODUCER_PRIO);
        inspector_task_h = Task_Create(inspector_task, "Inspector", 256, NULL, INSPECTOR_PRIO);
    }

    // --- �ź������� (��ӡ��) ---
    printer_semaphore = Semaphore_Create(2, 2);
    if (printer_semaphore) {
        printer_task1_h = Task_Create(printer_task, "PrinterTask1", 256, (void *) "PrinterTask1", PRINTER_TASK_PRIO);
        printer_task2_h = Task_Create(printer_task, "PrinterTask2", 256, (void *) "PrinterTask2", PRINTER_TASK_PRIO);
        printer_task3_h = Task_Create(printer_task, "PrinterTask3", 256, (void *) "PrinterTask3", PRINTER_TASK_PRIO);
    }

    // --- �жϰ�ȫAPI���� ---
    isr_semaphore = Semaphore_Create(10, 0);
    if (isr_semaphore) {
        isr_test_task_h = Task_Create(isr_test_task, "ISR_Test", 256, NULL, ISR_TEST_TASK_PRIO);
    }

    // --- ��������Э������ ---
    recursive_lock = Mutex_Create();
    if (recursive_lock) {
        recursive_task_h = Task_Create(recursive_test_task, "RecursiveTask", 128, NULL, COLLABORATION_TASKS_PRIO);
    }
    //
    a_task_h = Task_Create(a_task, "TaskA", 128, NULL, COLLABORATION_TASKS_PRIO);
    b_task_h = Task_Create(b_task, "TaskB", 128, NULL, COLLABORATION_TASKS_PRIO);
    // d_task_h = Task_Create(d_task, "TaskD_Creator", 256, NULL, COLLABORATION_TASKS_PRIO); //̫������ڴ��� ��ʱ�򴴽���˲�� run ָ���ܲ��� ��ע����
    background_task_h = Task_Create(background_blinky_task, "BG_Blinky_PB0", 64, NULL, BACKGROUND_TASK_PRIO);
    high_prio_task_h = Task_Create(high_prio_task, "HighPrioTask", 256, NULL, HIGH_PRIO_TASK_PRIO);
    interrupt_task_h = Task_Create(interrupt_handler_task, "KeyHandlerTask", 128, NULL, INTERRUPT_TASK_PRIO);
    timer_test_task_h = Task_Create(timer_test_task, "TimerTest", 512, NULL, TIMER_TEST_TASK_PRIO);
}


/*===========================================================================*
 *                            �жϷ������                                   *
 *===========================================================================*/
void EXTI0_IRQHandler(void) {
    if (exti_interrupt_flag_get(EXTI_0) != RESET) {
        exti_interrupt_flag_clear(EXTI_0);
        int higherPriorityTaskWoken = 0;

        if (interrupt_task_h != NULL) {
            Task_NotifyFromISR(interrupt_task_h, &higherPriorityTaskWoken);
        }
        if (isr_semaphore != NULL) {
            Semaphore_GiveFromISR(isr_semaphore, &higherPriorityTaskWoken);
        }

        MyRTOS_Port_YieldFromISR(higherPriorityTaskWoken);
    }
}


/*===========================================================================*
 *                             ����ڵ�                                      *
 *===========================================================================*/
int main(void) {
    //��ʼ��ƽ̨�� (���ᴦ�����еײ�ϸ�ں�RTOS����)
    Platform_Init();

    //��ӡ��Ϣ
    LOG_I("Main", "=========   MyRTOS ��ʾ   =========");
    LOG_I("Main", "|  ����: XiaoXiu");
    LOG_I("Main", "|  �����û������ɴ����ж�");
    LOG_I("Main", "===============================================");
    LOG_I("Main", "ϵͳ������...");

    //����RTOS������ ͨ��ƽ̨����
    //�Լ����ʹ�� Task_StartScheduler
    Platform_StartScheduler();
    return 0; // ��Զ����ִ�е�����
}
