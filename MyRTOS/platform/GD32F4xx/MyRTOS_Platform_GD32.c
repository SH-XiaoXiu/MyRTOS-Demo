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

// 新增的信号量，用于阻塞式读取
static SemaphoreHandle_t g_rx_char_sem = NULL;

#define MYRTOS_PORT_USARTx      USART0

void MyRTOS_Platform_Init(void) {
    lib_usart0_init();
    // 在系统启动时创建计数信号量，容量与缓冲区大小相同
    // 初始值为0，表示没有字符可用
    g_rx_char_sem = Semaphore_Create(RX_BUFFER_SIZE, 0);
    if (g_rx_char_sem == NULL) {
        // 致命错误，无法创建信号量，系统无法继续
        while(1);
    }

    usart_interrupt_enable(MYRTOS_PORT_USARTx, USART_INT_RBNE);
    // 确保中断优先级低于 configMAX_SYSCALL_INTERRUPT_PRIORITY
    // 对于 ARM Cortex-M，数值越大，优先级越低
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
        return 0; // 缓冲区为空
    }
    *c = rx_buffer[rx_buffer_tail];
    rx_buffer_tail = (rx_buffer_tail + 1) % RX_BUFFER_SIZE;
    MyRTOS_Port_EXIT_CRITICAL();
    return 1;
}

int MyRTOS_Platform_GetChar_Blocking(char *c) {
    // 无限期等待信号量，表示有新字符到达
    if (Semaphore_Take(g_rx_char_sem, MY_RTOS_MAX_DELAY)) {
        // 从缓冲区安全地读取字符
        // 因为 Take 成功意味着至少有一个字符，所以这里 MyRTOS_Platform_GetChar 总是会成功
        return MyRTOS_Platform_GetChar(c);
    }
    return 0; // 理论上不会发生，除非信号量Take失败
}

void USART0_IRQHandler(void) {
    BaseType_t higherPriorityTaskWoken = 0;
    // 检查是否是正常的数据接收 (RBNE)
    if (usart_interrupt_flag_get(MYRTOS_PORT_USARTx, USART_INT_FLAG_RBNE)) {
        char data = (char) usart_data_receive(MYRTOS_PORT_USARTx);
        uint16_t next_head = (rx_buffer_head + 1) % RX_BUFFER_SIZE;

        // 检查缓冲区是否已满
        if (next_head != rx_buffer_tail) {
            rx_buffer[rx_buffer_head] = data;
            rx_buffer_head = next_head;

            // 成功接收到字符后, 释放信号量
            if (g_rx_char_sem != NULL) {
                Semaphore_GiveFromISR(g_rx_char_sem, &higherPriorityTaskWoken);
            }
        }
        // 如果环形缓冲区满了, 新数据会被丢弃，这是一种健壮的策略
    }

    // (可选但推荐) 处理其他错误
    if (usart_interrupt_flag_get(MYRTOS_PORT_USARTx, USART_INT_FLAG_ERR_ORERR)) {
        usart_data_receive(MYRTOS_PORT_USARTx); // 清除 ORE 标志
    }

    // 如果 GiveFromISR 唤醒了一个更高优先级的任务，则请求一次上下文切换
    if (higherPriorityTaskWoken) {
        MyRTOS_Port_YIELD();
    }
}