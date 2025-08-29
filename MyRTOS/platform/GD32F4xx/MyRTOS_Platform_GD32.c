//
// Created by XiaoXiu on 8/29/2025.
//
// Platform: GD32F4xx Series
//

#include "gd32f4xx_usart.h"
#include "MyRTOS.h"
#include "MyRTOS_Platform.h"
#include "lib_usart0.h"
#include "gd32f4xx_misc.h"
#include "MyRTOS_Port.h"

#define RX_BUFFER_SIZE 64
static volatile char rx_buffer[RX_BUFFER_SIZE];
static volatile uint16_t rx_buffer_head = 0;
static volatile uint16_t rx_buffer_tail = 0;

// �������ź�������������ʽ��ȡ
static SemaphoreHandle_t g_rx_char_sem = NULL;

#define MYRTOS_PORT_USARTx      USART0

void MyRTOS_Platform_Init(void) {
    lib_usart0_init();
    // ��ϵͳ����ʱ���������ź����������뻺������С��ͬ
    // ��ʼֵΪ0����ʾû���ַ�����
    g_rx_char_sem = Semaphore_Create(RX_BUFFER_SIZE, 0);
    if (g_rx_char_sem == NULL) {
        // ���������޷������ź�����ϵͳ�޷�����
        while(1);
    }

    usart_interrupt_enable(MYRTOS_PORT_USARTx, USART_INT_RBNE);
    // ȷ���ж����ȼ����� configMAX_SYSCALL_INTERRUPT_PRIORITY
    // ���� ARM Cortex-M����ֵԽ�����ȼ�Խ��
    NVIC_SetPriority(USART0_IRQn, 6);
    NVIC_EnableIRQ(USART0_IRQn);
}

void MyRTOS_Platform_PutChar(char c) {
    usart_data_transmit(MYRTOS_PORT_USARTx, (uint8_t) c);
    while (RESET == usart_flag_get(MYRTOS_PORT_USARTx, USART_FLAG_TC));
}

int MyRTOS_Platform_GetChar(char *c) {
    MyRTOS_Port_ENTER_CRITICAL();
    if (rx_buffer_head == rx_buffer_tail) {
        MyRTOS_Port_EXIT_CRITICAL();
        return 0; // ������Ϊ��
    }
    *c = rx_buffer[rx_buffer_tail];
    rx_buffer_tail = (rx_buffer_tail + 1) % RX_BUFFER_SIZE;
    MyRTOS_Port_EXIT_CRITICAL();
    return 1;
}

int MyRTOS_Platform_GetChar_Blocking(char *c) {
    // �����ڵȴ��ź�������ʾ�����ַ�����
    if (Semaphore_Take(g_rx_char_sem, MY_RTOS_MAX_DELAY)) {
        // �ӻ�������ȫ�ض�ȡ�ַ�
        // ��Ϊ Take �ɹ���ζ��������һ���ַ����������� MyRTOS_Platform_GetChar ���ǻ�ɹ�
        return MyRTOS_Platform_GetChar(c);
    }
    return 0; // �����ϲ��ᷢ���������ź���Takeʧ��
}

void USART0_IRQHandler(void) {
    BaseType_t higherPriorityTaskWoken = 0;
    // ����Ƿ������������ݽ��� (RBNE)
    if (usart_interrupt_flag_get(MYRTOS_PORT_USARTx, USART_INT_FLAG_RBNE)) {
        char data = (char) usart_data_receive(MYRTOS_PORT_USARTx);
        uint16_t next_head = (rx_buffer_head + 1) % RX_BUFFER_SIZE;

        // ��黺�����Ƿ�����
        if (next_head != rx_buffer_tail) {
            rx_buffer[rx_buffer_head] = data;
            rx_buffer_head = next_head;

            // �ɹ����յ��ַ���, �ͷ��ź���
            if (g_rx_char_sem != NULL) {
                Semaphore_GiveFromISR(g_rx_char_sem, &higherPriorityTaskWoken);
            }
        }
        // ������λ���������, �����ݻᱻ����������һ�ֽ�׳�Ĳ���
    }

    // (��ѡ���Ƽ�) ������������
    if (usart_interrupt_flag_get(MYRTOS_PORT_USARTx, USART_INT_FLAG_ERR_ORERR)) {
        usart_data_receive(MYRTOS_PORT_USARTx); // ��� ORE ��־
    }

    // ��� GiveFromISR ������һ���������ȼ�������������һ���������л�
    if (higherPriorityTaskWoken) {
        MyRTOS_Port_YIELD();
    }
}