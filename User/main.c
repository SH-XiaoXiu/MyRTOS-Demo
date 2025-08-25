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

Mutex_t print_lock;
Task_t *a_task_h = NULL;
Task_t *b_task_h = NULL;
Task_t *c_task_h = NULL; // �� d_task ����
Task_t *d_task_h = NULL;
Task_t *e_task_h = NULL;
Task_t *high_prio_task_h = NULL;
Task_t *interrupt_task_h = NULL;
Task_t *background_task_h = NULL;


void a_task(void *param) {
    static uint8_t i = 0;
    Mutex_Lock(&print_lock);
    printf("���� A (Prio %d) ����\n", COLLABORATION_TASKS_PRIO);
    Mutex_Unlock(&print_lock);
    while (1) {
        Task_Wait();
        while (1) {
            i++;
            Mutex_Lock(&print_lock);
            printf("���� A ��������,�� %d ��\n", i);
            Mutex_Unlock(&print_lock);
            if (i == 5) {
                i = 0;
                Mutex_Lock(&print_lock);
                printf("���� A ���� ���� B, ����ʼ�ȴ� ���� B �Ļ���\n");
                Mutex_Unlock(&print_lock);
                Task_Notify(b_task_h->taskId);
                break;
            }
            Task_Delay(1000); //1s
        }
    }
}

void b_task(void *param) {
    static uint8_t i = 0;
    Mutex_Lock(&print_lock);
    printf("���� B (Prio %d) ����\n", COLLABORATION_TASKS_PRIO);
    Mutex_Unlock(&print_lock);
    while (1) {
        Task_Wait();
        while (1) {
            Mutex_Lock(&print_lock);
            printf("���� B ��������,�� %d ��\n", i);
            Mutex_Unlock(&print_lock);
            i++;
            if (i == 3) {
                i = 0;
                Mutex_Lock(&print_lock);
                printf("���� B ���� ���� A, ����ʼ�ȴ� ���� A �Ļ���\n");
                Mutex_Unlock(&print_lock);
                Task_Notify(a_task_h->taskId);
                break;
            }
            Task_Delay(1000); //1s
        }
    }
}

void c_task(void *param) {
    uint32_t index = 0;
    Mutex_Lock(&print_lock);
    printf("���� C (Prio %d) ����\n", COLLABORATION_TASKS_PRIO);
    Mutex_Unlock(&print_lock);
    while (1) {
        index++;
        Mutex_Lock(&print_lock);
        printf("���� C ��������,�� %d ��\n", index);
        Mutex_Unlock(&print_lock);
        if (index == 5) {
            Mutex_Lock(&print_lock);
            printf("���� C ɾ�� ���� C\n");
            Mutex_Unlock(&print_lock);
            // ������ɾ������������ʧЧ
            Task_Delete(c_task_h);
        }
        Task_Delay(1000);
    }
}

void d_task(void *param) {
    uint32_t index = 0;
    Mutex_Lock(&print_lock);
    printf("���� D (Prio %d) ����\n", COLLABORATION_TASKS_PRIO);
    Mutex_Unlock(&print_lock);
    while (1) {
        // ��� c_task_h �Ƿ�Ϊ NULL ��������ɾ��
        if (c_task_h == NULL || c_task_h->state == TASK_STATE_UNUSED) {
            index++;
            if (index >= 5) {
                Mutex_Lock(&print_lock);
                printf("���� D �� ���� ���� C\n");
                Mutex_Unlock(&print_lock);
                c_task_h = Task_Create(c_task, NULL, COLLABORATION_TASKS_PRIO);
                index = 0;
            }
        }
        Mutex_Lock(&print_lock);
        printf("���� D ��������, ������ %d\n", index);
        Mutex_Unlock(&print_lock);
        Task_Delay(1000);
    }
}

void e_task(void *param) {
    Mutex_Lock(&print_lock);
    printf("���� E (Prio %d) ����, ��˸ PB2\n", COLLABORATION_TASKS_PRIO);
    Mutex_Unlock(&print_lock);
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
    Mutex_Lock(&print_lock);
    printf("��̨���� (Prio %d) ����, ��˸ PB0\n", BACKGROUND_TASK_PRIO);
    Mutex_Unlock(&print_lock);
    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_0);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_0);
    while (1) {
        gpio_bit_toggle(GPIOB, GPIO_PIN_0);
        Task_Delay(1000);
    }
}

void high_prio_task(void *param) {
    Mutex_Lock(&print_lock);
    printf("�������� (Prio %d) ����\n", HIGH_PRIO_TASK_PRIO);
    Mutex_Unlock(&print_lock);
    while (1) {
        Task_Delay(5000); // ÿ 5 ����ռһ��
        Mutex_Lock(&print_lock);
        printf("\n<<<<<<<<<< [�����ȼ�������ռ] >>>>>>>>>>\n\n");
        Mutex_Unlock(&print_lock);
    }
}

void interrupt_handler_task(void *param) {
    Mutex_Lock(&print_lock);
    printf("�ж����� (Prio %d) ����, �ȴ�����...\n", INTERRUPT_TASK_PRIO);
    Mutex_Unlock(&print_lock);
    while (1) {
        Task_Wait();
        Mutex_Lock(&print_lock);
        printf("\n\n!!!!!!!!!! [�жϽ����¼�����] !!!!!!!!!!\n\n");
        Mutex_Unlock(&print_lock);
    }
}

void boot_task(void *param) {
    a_task_h = Task_Create(a_task, NULL, COLLABORATION_TASKS_PRIO);
    b_task_h = Task_Create(b_task, NULL, COLLABORATION_TASKS_PRIO);
    d_task_h = Task_Create(d_task, NULL, COLLABORATION_TASKS_PRIO);
    e_task_h = Task_Create(e_task, NULL, COLLABORATION_TASKS_PRIO);
    background_task_h = Task_Create(background_blinky_task, NULL, BACKGROUND_TASK_PRIO);
    high_prio_task_h = Task_Create(high_prio_task, NULL, HIGH_PRIO_TASK_PRIO);
    interrupt_task_h = Task_Create(interrupt_handler_task, NULL, INTERRUPT_TASK_PRIO);
    Task_Delay(200);
    Task_Notify(a_task_h->taskId);
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
            Task_Notify(interrupt_task_h->taskId);
        }
        exti_interrupt_flag_clear(EXTI_0);
    }
}

void sys_config() {
    lib_usart0_init();
    Mutex_Init(&print_lock);
}


int main(void) {
    sys_config();
    key_exti_init();
    printf("=========   RTOS Priority Demo   =========\n");
    printf("|  Author: XiaoXiu                   \n");
    printf("|  Version: 1.0 (Priority-based)     \n");
    printf("|  MCU: GD32                         \n");
    printf("==========================================\n");
    Task_Create(boot_task, NULL, 0);
    printf("System Starting...\n");
    Task_StartScheduler();
    while (1) {
    };
}
