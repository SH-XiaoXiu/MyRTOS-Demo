#include <stdint.h>
#include <stdio.h>

#include "lib_usart0.h"
#include "MyRTOS.h"

#define A_TASK 0
#define B_TASK 1
#define C_TASK 2

Mutex_t print_lock;

Task_t *a_task_h;
Task_t *b_task_h;
Task_t *c_task_h;

void a_task(void *param) {
    static uint8_t i = 0;
    Mutex_Lock(&print_lock); //Ϊ������ʱ��ӡ���� ����
    printf("���� A ����\n");
    Mutex_Unlock(&print_lock);
    while (1) {
        Task_Wait();
        while (1) {
            printf("A ������������,�� %d ��\n", i);
            i++;
            if (i == 5) {
                i = 0;
                printf("���� A ���� ���� B, ����ʼ�ȴ� ���� B �Ļ���\n");
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
    printf("���� B ����\n");
    Mutex_Unlock(&print_lock);
    while (1) {
        Task_Wait();
        while (1) {
            printf("B ������������,�� %d ��\n", i);
            i++;
            if (i == 3) {
                i = 0;
                printf("���� B ���� ���� A, ����ʼ�ȴ� ���� A �Ļ���\n");
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
    printf("���� C ����\n");
    printf("���� C ���� ���� A\n");
    Mutex_Unlock(&print_lock);
    Task_Notify(a_task_h->taskId);
    while (1) {
        index++;
        printf("���� C ��������,�� %d ��\n", index);
        Task_Delay(2000);
    }
}


void sys_config() {
    lib_usart0_init();
    // ��ʼ��������
    Mutex_Init(&print_lock);
}


/**
 * @brief һ��ARM32��RTOS����ʵ��
 * @author A_XiaoXiu
 * @remark �ο�����
 * \n
 * FreeRTOS: https://github.com/FreeRTOS/FreeRTOS-Kernel
 * \n
 * AI: https://chatgpt.com/ -> PSP��ಿ��
 * �����ʵ��, ������ɾ��, ���������ȼ�
 * ���Ż�����
 * @return void
 */
int main(void) {
    sys_config();
    printf("=========   =========================\n");
    printf("|        RTOS Demo          \n");
    printf("|  Author: XiaoXiu          \n");
    printf("|  Version: 0.0             \n");
    printf("|  MCU: GD32                \n");
    printf("==================================\n");
    a_task_h = Task_Create(a_task, NULL);
    b_task_h = Task_Create(b_task, NULL);
    c_task_h = Task_Create(c_task, NULL);
    printf("System Starting...\n");
    Task_StartScheduler();
    while (1) {
    };
}
