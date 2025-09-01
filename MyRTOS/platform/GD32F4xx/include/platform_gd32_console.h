//
// Created by XiaoXiu on 8/30/2025.
//

#ifndef PLATFORM_GD32_CONSOLE_H
#define PLATFORM_GD32_CONSOLE_H

#include "MyRTOS_Stream_Def.h"


/**
 * @brief ����ʱ,���һ���ַ�������̨���޹�RTOS
 * @param c Ҫ��ӡ���ַ�
 */
void Platform_fault_putchar(char c);

/**
 * @brief ��ʼ��GD32ƽ̨�ĵ��Կ���̨��
 *
 * �������Ӧ����Ӳ����ʼ��֮��RTOS����֮ǰ���á�
 * ��������USART0Ӳ����
 */
void Platform_Console_HwInit(void);

/**
 * @brief ��ȡһ��ָ���ѳ�ʼ���Ŀ���̨���ľ����
 *
 * �ڵ��� GD32_Console_HwInit() ֮��Ӧ�ó������ͨ���˺�����ȡ
 * һ��ʵ���� Stream �ӿڵľ�����������ӵ�RTOS��StdIO����
 *
 * @return StreamHandle_t ָ�����̨���ľ����
 */
StreamHandle_t Platform_Console_GetStream(void);


#endif // PLATFORM_GD32_CONSOLE_H
