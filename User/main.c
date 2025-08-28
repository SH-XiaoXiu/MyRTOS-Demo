#include <stdint.h>
#include <stdio.h>

#include "gd32f4xx_gpio.h"
#include "gd32f4xx_rcu.h"
#include "gd32f4xx_exti.h"
#include "gd32f4xx_syscfg.h"
#include "lib_usart0.h"
#include "MyRTOS.h"

#define BACKGROUND_TASK_PRIO       1  // ��̨����
#define COLLABORATION_TASKS_PRIO   2  // Э��������
#define HIGH_PRIO_TASK_PRIO        3  // �����ȼ���������
#define INTERRUPT_TASK_PRIO        4  // �ж���Ӧ���� ���ȼ����


#define PRODUCER_PRIO       2  // �������������ȼ� (��Э��������ͬ)
#define CONSUMER_PRIO       1  // �������������ȼ� (���̨������ͬ���ϵ�)
#define INSPECTOR_PRIO      4  // �ʼ�Ա�������ȼ� (���ж�������ͬ���ܸ�)

typedef struct {
    uint32_t id;
    uint32_t data;
} Product_t;

QueueHandle_t product_queue; // ��Ϣ���о��

TaskHandle_t producer_task_h = NULL;
TaskHandle_t consumer_task_h = NULL;
TaskHandle_t inspector_task_h = NULL;

MutexHandle_t print_lock;
TaskHandle_t a_task_h = NULL;
TaskHandle_t b_task_h = NULL;
TaskHandle_t c_task_h = NULL; // �� d_task ����
TaskHandle_t d_task_h = NULL;
TaskHandle_t e_task_h = NULL;
TaskHandle_t high_prio_task_h = NULL;
TaskHandle_t interrupt_task_h = NULL;
TaskHandle_t background_task_h = NULL;
TimerHandle_t perio_timer_h = NULL;
TimerHandle_t single_timer_h = NULL;

MutexHandle_t recursive_lock; // ���Եݹ���
TaskHandle_t recursive_task_h = NULL; // ���Եݹ���������

static int scheduler_started = 0;

#define SAFE_PRINTF(...)                                \
do {                                                    \
    if (scheduler_started) {                            \
        Mutex_Lock(print_lock);                         \
        printf(__VA_ARGS__);                            \
        Mutex_Unlock(print_lock);                       \
    } else {                                            \
        printf(__VA_ARGS__);                            \
    }                                                   \
} while (0)


void perio_timer_cb(TimerHandle_t timer) {
    SAFE_PRINTF("������ ��ʱ�� ִ��: %llu\n", MyRTOS_GetTick());
}

void single_timer_cb(TimerHandle_t timer) {
    SAFE_PRINTF("���� ��ʱ�� ִ��: %llu\n", MyRTOS_GetTick());
}

void a_task(void *param) {
    static uint16_t i = 0;

    SAFE_PRINTF("���� A (Prio %d) ����\n", COLLABORATION_TASKS_PRIO);

    while (1) {
        Task_Wait();
        while (1) {
            i++;

            SAFE_PRINTF("���� A: ���Ի�ȡ�ݹ���...\n");


            Mutex_Lock_Recursive(recursive_lock); // ʹ����ͨ Lock Ҳ�У�����Ϊ��ͳһ


            SAFE_PRINTF("���� A: �ɹ���ȡ�ݹ��� ����һ��, Ȼ���ͷš�\n");


            Mutex_Unlock_Recursive(recursive_lock);


            SAFE_PRINTF("���� A ��������,�� %d ��\n", i);


            if (i == 5) {
                i = 0;

                SAFE_PRINTF("���� A ���� ���� B, ����ʼ�ȴ� ���� B �Ļ���\n");

                Task_Notify(b_task_h);
                break;
            }
            Task_Delay(1000); //1s
        }
    }
}

void b_task(void *param) {
    static uint16_t i = 0;

    SAFE_PRINTF("���� B (Prio %d) ����\n", COLLABORATION_TASKS_PRIO);

    while (1) {
        Task_Wait();
        while (1) {
            SAFE_PRINTF("���� B ��������,�� %d ��\n", i);

            i++;
            if (i == 3) {
                i = 0;

                SAFE_PRINTF("���� B ���� ���� A, ����ʼ�ȴ� ���� A �Ļ���\n");

                Task_Notify(a_task_h);
                break;
            }
            Task_Delay(1000); //1s
        }
    }
}

void c_task(void *param) {
    uint16_t index = 0;

    SAFE_PRINTF("���� C (Prio %d) ����\n", COLLABORATION_TASKS_PRIO);

    while (1) {
        index++;

        SAFE_PRINTF("���� C ��������,�� %d ��\n", index);

        if (index == 5) {
            SAFE_PRINTF("���� C ɾ�� ���� C\n");

            // ������ɾ������������ʧЧ
            Task_Delete(c_task_h);
        }
        Task_Delay(1000);
    }
}

void d_task(void *param) {
    uint16_t index = 0;

    SAFE_PRINTF("���� D (Prio %d) ����\n", COLLABORATION_TASKS_PRIO);

    while (1) {
        // ��� c_task_h �Ƿ�Ϊ NULL ��������ɾ��
        if (c_task_h == NULL || Task_GetState(c_task_h) == TASK_STATE_UNUSED) {
            index++;
            if (index >= 5) {
                SAFE_PRINTF("���� D �� ���� ���� C\n");

                c_task_h = Task_Create(c_task, 256,NULL, COLLABORATION_TASKS_PRIO);
                index = 0;
            }
        }

        SAFE_PRINTF("���� D ��������, ������ %d\n", index);

        Task_Delay(1000);
    }
}

void e_task(void *param) {
    SAFE_PRINTF("���� E (Prio %d) ����, ��˸ PB2\n", COLLABORATION_TASKS_PRIO);

    rcu_periph_clock_enable(RCU_GPIOB);
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_2);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_2);
    GPIO_BC(GPIOB) = GPIO_PIN_2;
    while (1) {
        gpio_bit_set(GPIOB, GPIO_PIN_2);
        Task_Delay(250);
        gpio_bit_reset(GPIOB, GPIO_PIN_2);
        Task_Delay(250);
    }
}


// ��̨����, ��˸��һ��LED (PB0)
void background_blinky_task(void *param) {
    SAFE_PRINTF("��̨���� (Prio %d) ����, ��˸ PB0\n", BACKGROUND_TASK_PRIO);

    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_0);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_0);
    while (1) {
        gpio_bit_toggle(GPIOB, GPIO_PIN_0);
        Task_Delay(1000);
    }
}

void high_prio_task(void *param) {
    SAFE_PRINTF("�������� (Prio %d) ����\n", HIGH_PRIO_TASK_PRIO);

    while (1) {
        Task_Delay(5000); // ÿ 5 ����ռһ��

        SAFE_PRINTF("\n<<<<<<<<<< [�����ȼ�������ռ] >>>>>>>>>>\n\n");
    }
}

void interrupt_handler_task(void *param) {
    SAFE_PRINTF("�����ж� (Prio %d) ����, �ȴ�����...\n", INTERRUPT_TASK_PRIO);

    while (1) {
        Task_Wait();

        SAFE_PRINTF("\n\n!!!!!!!!!! [�жϽ����¼�����] !!!!!!!!!!\n\n");
    }
}


void producer_task(void *param) {
    Product_t product;
    product.id = 0;
    product.data = 100;

    SAFE_PRINTF("������ ���� (Prio %d)", PRODUCER_PRIO);

    while (1) {
        product.id++;
        product.data += 10;

        SAFE_PRINTF("������: ���Ͳ�Ʒ ID %d\n", product.id);

        // ���Ͳ�Ʒ�����У�����������˻������ȴ�
        if (Queue_Send(product_queue, &product, MY_RTOS_MAX_DELAY)) {
            SAFE_PRINTF("������: �ɹ����Ͳ�Ʒ ID %d\n", product.id);
        } else {
            SAFE_PRINTF("������: ��������, �������Ͳ�Ʒ ID %d\n", product.id);
        }
        Task_Delay(2000); // ÿ 2 ������һ��
    }
}

/**
 * @brief ���������� (�����ȼ�)
 */
void consumer_task(void *param) {
    Product_t received_product;

    SAFE_PRINTF("������ ���� (Prio %d)", CONSUMER_PRIO);

    while (1) {
        SAFE_PRINTF("������: �ȴ����ղ�Ʒ...\n");

        // �Ӷ��н��ղ�Ʒ���������Ϊ�ջ������ȴ�
        if (Queue_Receive(product_queue, &received_product, MY_RTOS_MAX_DELAY)) {
            SAFE_PRINTF("������: ���յ���Ʒ ID %d\n", received_product.id);
        } else {
            SAFE_PRINTF("������: �����ѿ�, �������ղ�Ʒ\n");
        }
        // û����ʱ��������һ�����Ͼ�ȥ����һ��
    }
}

/**
 * @brief �ʼ�Ա���� (�����ȼ�)
 */
void inspector_task(void *param) {
    Product_t received_product;


    SAFE_PRINTF("�ʼ�Ա ���� (Prio %d)", INSPECTOR_PRIO);


    while (1) {
        SAFE_PRINTF("�ʼ�Ա: �ȴ����ղ�Ʒ...\n");

        // ͬ���Ӷ��н��գ�����Ϊ���ȼ��ߣ��������ȱ�����
        if (Queue_Receive(product_queue, &received_product, MY_RTOS_MAX_DELAY)) {
            SAFE_PRINTF("�ʼ�Ա: ���� ����Ʒ ID %d\n", received_product.id);
        } else {
            SAFE_PRINTF("�ʼ�Ա: �����ѿ�, ���� ���� ��Ʒ\n");
        }
        // �ʼ�Ա��������Ϣ 5 �룬��������һЩ����
        Task_Delay(5000);
    }
}


void sub_function_needs_lock(int level) {
    SAFE_PRINTF("�ݹ����� �Ӻ��� %d ����\n", level);

    Mutex_Lock_Recursive(recursive_lock);

    SAFE_PRINTF("�ݹ����� �Ӻ��� %d �Ѽ���\n", level);

    Task_Delay(500);

    SAFE_PRINTF("�ݹ����� �Ӻ��� %d ����\n", level);
    Mutex_Unlock_Recursive(recursive_lock);
}

void recursive_test_task(void *param) {
    SAFE_PRINTF("�ݹ����� ���� (Prio %d)\n", COLLABORATION_TASKS_PRIO);

    while (1) {
        SAFE_PRINTF("�ݹ����� ��ѭ�� ��ʼ���� (3��)\n");

        Mutex_Lock_Recursive(recursive_lock);
        SAFE_PRINTF("�ݹ����� ��ѭ�� ���� (1��)\n");

        sub_function_needs_lock(2);
        sub_function_needs_lock(3);

        SAFE_PRINTF("�ݹ����� ��ѭ�� ���� (1��)\n");
        Mutex_Unlock_Recursive(recursive_lock);

        SAFE_PRINTF("�ݹ����� ����ȫ���� ��ʱ 3�� �������������\n");

        Task_Delay(3000);

        SAFE_PRINTF("�ݹ����� ��ʱ����\n");

        Task_Delay(2000);
    }
}



void boot_task(void *param) {
    Task_Create(recursive_test_task, 512, NULL, COLLABORATION_TASKS_PRIO);
    a_task_h = Task_Create(a_task, 256, NULL, COLLABORATION_TASKS_PRIO);
    b_task_h = Task_Create(b_task, 256,NULL, COLLABORATION_TASKS_PRIO);
    d_task_h = Task_Create(d_task, 256,NULL, COLLABORATION_TASKS_PRIO);
    e_task_h = Task_Create(e_task, 256, NULL, COLLABORATION_TASKS_PRIO);
    background_task_h = Task_Create(background_blinky_task, 256, NULL, BACKGROUND_TASK_PRIO);
    high_prio_task_h = Task_Create(high_prio_task, 256,NULL, HIGH_PRIO_TASK_PRIO);
    interrupt_task_h = Task_Create(interrupt_handler_task, 256,NULL, INTERRUPT_TASK_PRIO);
    Task_Delay(200);
    Task_Notify(a_task_h);


    SAFE_PRINTF("���� ����\n");

    // ����һ�������� 3 �� Product_t �Ķ���
    product_queue = Queue_Create(3, sizeof(Product_t));
    if (product_queue == NULL) {
        SAFE_PRINTF("���д���ʧ��\n");
    }
    // ����������ص�����
    consumer_task_h = Task_Create(consumer_task, 256,NULL, CONSUMER_PRIO);
    producer_task_h = Task_Create(producer_task, 256,NULL, PRODUCER_PRIO);
    inspector_task_h = Task_Create(inspector_task, 256, NULL, INSPECTOR_PRIO);
    //������ʱ��
    perio_timer_h = Timer_Create(10000, 10000, perio_timer_cb, NULL);
    single_timer_h = Timer_Create(5000, 0, single_timer_cb, NULL);


    SAFE_PRINTF("��ʱ�� ����\n");


    Timer_Start(perio_timer_h);
    Timer_Start(single_timer_h);


    SAFE_PRINTF("=============================================\n\n");

    Task_Delete(NULL);
}


void key_exti_init(void) {
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_SYSCFG);
    gpio_mode_set(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_0);
    syscfg_exti_line_config(EXTI_SOURCE_GPIOA, EXTI_SOURCE_PIN0);
    exti_init(EXTI_0, EXTI_INTERRUPT, EXTI_TRIG_FALLING);
    exti_interrupt_flag_clear(EXTI_0);
    NVIC_SetPriority(EXTI0_IRQn, 5);
    NVIC_EnableIRQ(EXTI0_IRQn);
}

void EXTI0_IRQHandler(void) {
    if (exti_interrupt_flag_get(EXTI_0) != RESET) {
        if (interrupt_task_h != NULL) {
            Task_Notify(interrupt_task_h);
        }
        exti_interrupt_flag_clear(EXTI_0);
    }
}

void sys_config() {
    lib_usart0_init();
    print_lock = Mutex_Create();
    recursive_lock = Mutex_Create();
    if (recursive_lock == NULL) {
        SAFE_PRINTF("�ݹ�������ʧ��!\n");
    }
}


int main(void) {
    sys_config();
    key_exti_init();
    SAFE_PRINTF("=========   RTOS Demo   =========\n");
    SAFE_PRINTF("|  Author: XiaoXiu                   \n");
    SAFE_PRINTF("|  Version: 1.0 (Priority-based)     \n");
    SAFE_PRINTF("|  MCU: GD32                         \n");
    SAFE_PRINTF("==========================================\n");
    Task_Create(boot_task, 256, NULL, 0);
    SAFE_PRINTF("System Starting...\n");
    Task_StartScheduler();
    while (1) {
    };
}
